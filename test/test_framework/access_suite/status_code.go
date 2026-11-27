package access_suite

import (
	"fmt"
	"net/http"
	"sync/atomic"
	"test_framework/common"
	"test_framework/config"
)

var statusCodeOriginHits int32

func handleStatusCode(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Cache-Control", "public, max-age=3600")
	if atomic.AddInt32(&statusCodeOriginHits, 1) > 1 {
		w.WriteHeader(http.StatusInternalServerError)
		_, _ = w.Write([]byte("origin-called-again"))
		return
	}
	_, _ = w.Write([]byte("origin-content"))
}

func check_status_code() {
	host := config.GetLocalhost("access")
	path := fmt.Sprintf("/status-code?alpn=%d", config.Cfg.Alpn)
	atomic.StoreInt32(&statusCodeOriginHits, 0)
	common.Getx(path, host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(common.Read(resp), "origin-content")
	})
	common.Getx(path, host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(common.Read(resp), "origin-content")
	})
	common.AssertSame(atomic.LoadInt32(&statusCodeOriginHits), int32(1))
	common.Getx(path, host, map[string]string{"X-Test-Status-Code": "1"}, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, 451)
		common.AssertSame(common.Read(resp), "status-code-error-page\n")
	})
}
