package access_suite

import (
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync/atomic"
	"test_framework/common"
	"test_framework/config"

	"github.com/quic-go/quic-go/http3"
	"golang.org/x/net/http2"
)

var guestCacheHits int32

func handleLegacyFilter(w http.ResponseWriter, r *http.Request) {
	switch {
	case r.URL.Path == "/filters/replace-content":
		_, _ = w.Write([]byte(strings.Repeat("x", 8190) + "foo123 after"))
	case r.URL.Path == "/filters/replace-url":
		w.Header().Set("Content-Type", "text/html")
		_, _ = w.Write([]byte(`<a href="http://old.test/page">link</a>`))
	case r.URL.Path == "/filters/replace-location":
		w.Header().Set("Location", "http://old.test/redirected")
		w.WriteHeader(http.StatusFound)
	case r.URL.Path == "/filters/content-deny":
		_, _ = w.Write([]byte("prefix blocked-value suffix"))
	case r.URL.Path == "/filters/status":
		_, _ = w.Write([]byte("status-body"))
	case r.URL.Path == "/filters/range":
		w.Header().Set("Content-Range", "bytes 2-4/10")
		w.WriteHeader(http.StatusPartialContent)
		_, _ = w.Write([]byte("cde"))
	case len(r.URL.Path) >= len("/filters/url-range/"):
		_, _ = w.Write([]byte(r.Header.Get("Range")))
	}
}

func handleGuestCache(w http.ResponseWriter, r *http.Request) {
	atomic.AddInt32(&guestCacheHits, 1)
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Set-Cookie", "private=1")
	_, _ = w.Write([]byte("guest-cache-body"))
}

func check_legacy_filters() {
	host := config.GetLocalhost("access")
	common.Getx("/filters/replace-content", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		content := common.Read(resp)
		common.AssertSame(len(content), 8190+len("bar123 after"))
		common.Assert("replace-content across buffers", strings.HasSuffix(content, "bar123 after"))
	})
	common.Getx("/filters/replace-url", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(common.Read(resp), `<a href="https://new.test/page">link</a>`)
	})
	if config.Cfg.Alpn == config.HTTP1 {
		common.Getx("/filters/replace-location", host, nil, func(resp *http.Response, err error) {
			common.Assert("replace-location response", resp != nil)
			common.AssertSame(resp.StatusCode, http.StatusFound)
			common.AssertSame(resp.Header.Get("Location"), "https://new.test/redirected")
		})
	}
	common.Getx("/filters/status", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusTeapot)
		common.AssertSame(common.Read(resp), "status-body")
	})
	common.Getx("/filters/range", host, map[string]string{"Range": "bytes=2-4"}, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusPartialContent)
		common.AssertSame(common.Read(resp), "HDRcde")
	})
	common.Getx("/filters/url-range/2-4", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(common.Read(resp), "bytes=2-4")
	})

	atomic.StoreInt32(&guestCacheHits, 0)
	path := fmt.Sprintf("/filters/guest-cache?alpn=%d", config.Cfg.Alpn)
	for i := 0; i < 2; i++ {
		common.Getx(path, host, nil, func(resp *http.Response, err error) {
			common.AssertSame(err, nil)
			common.AssertSame(common.Read(resp), "guest-cache-body")
			common.AssertSame(resp.Header.Get("Set-Cookie"), "")
		})
	}
	common.AssertSame(atomic.LoadInt32(&guestCacheHits), int32(1))

	common.Getx("/filters/content-deny", host, nil, func(resp *http.Response, err error) {
		// A matched response-body deny may close HTTP/1 or reset HTTP/2/3 before
		// sending headers. This is intentional; never dereference a nil response.
		if err != nil {
			var streamErr http2.StreamError
			var h3Err *http3.Error
			common.Assert("content deny closes or resets the response", errors.Is(err, io.EOF) ||
				(errors.As(err, &streamErr) && (streamErr.Code == http2.ErrCodeInternal || streamErr.Code == http2.ErrCodeCancel)) ||
				(errors.As(err, &h3Err) && h3Err.Remote && h3Err.ErrorCode == http3.ErrCodeRequestCanceled))
			return
		}
		common.Assert("content deny response exists", resp != nil)
		if resp != nil {
			common.AssertSame(common.Read(resp), "")
		}
	})
	common.Getx("/builtin-filter/param?allowed=yes", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		if resp != nil {
			common.AssertSame(common.Read(resp), "builtin-filter-ok")
		}
	})
}
