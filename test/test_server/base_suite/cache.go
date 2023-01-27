package base_suite

import (
	"fmt"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/config"
	"test_server/kangle"
)

const (
	last_modified = "Thu, 14 Oct 2021 02:45:05 GMT"
	etag          = "\"hello\""
)

func handle_etag_last_modified(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()

	defer cn.Close()
	if_none_match := r.Header.Get("If-None-Match")
	if_modified_since := r.Header.Get("If-Modified-Since")
	if if_none_match != "" {
		if if_none_match == etag {
			wb.WriteString("HTTP/1.1 304 Not Modified\r\nContent-Length: 2\r\nConnection: close\r\n\r\n")
			wb.Flush()
			return
		}
	} else if if_modified_since != "" {
		if if_modified_since == last_modified {
			wb.WriteString("HTTP/1.1 304 Not Modified\r\nContent-Length: 2\r\nConnection: close\r\n\r\n")
			wb.Flush()
			return
		}
	}
	wb.WriteString("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\nLast-Modified: ")
	wb.WriteString(last_modified)
	wb.WriteString("\r\nEtag: ")
	wb.WriteString(etag)
	wb.WriteString("\r\nServer: test-server/1.0\r\n\r\nok")
	wb.Flush()
}
func handle_dynamic_etag(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	etag := r.Header.Get("x-etag")
	if if_none_match == "" || !common.MatchEtag(if_none_match, etag) {
		w.Header().Add("Server", TEST_SERVER_NAME)
		w.Header().Add("Etag", etag)
		w.Write([]byte(etag))
		return
	}

	w.Header().Add("Etag", etag)
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.WriteHeader(304)
}
func HandleBrokenCache(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	if if_none_match == "" {
		common.RequestCount++
		hj, _ := w.(http.Hijacker)
		cn, wb, _ := hj.Hijack()
		chunk := r.FormValue("chunk")
		if chunk == "1" {
			wb.WriteString("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nEtag: hello\r\nConnection: keep-alive\r\n\r\n4\r\nhell\r\n")
		} else {
			wb.WriteString("HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nEtag: hello\r\nContent-Length: 5\r\n\r\nhell")
		}
		wb.Flush()
		cn.Close()
		return
	}
	common.Assert("if-none-match-1", if_none_match == "hello")
	common.RequestCount++
	w.WriteHeader(304)
	w.Header().Add("Server", TEST_SERVER_NAME)
}
func check_broken_no_cache() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	//带content-length
	common.Get("/broken_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/broken_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	//分块
	common.Get("/broken_cache?chunk=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/broken_cache?chunk=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
}
func check_disk_cache() {
	common.Get("/static/index.id?dc=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.Assert("last-modified", len(resp.Header.Get("Last-Modified")) > 0)
	})
	common.Get("/static/index.id?dc=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.Assert("last-modified", len(resp.Header.Get("Last-Modified")) > 0)
	})

	common.Get("/etag?disk_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(resp.Header.Get("Etag"), "hello")
	})
	common.Get("/etag?disk_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(resp.Header.Get("Etag"), "hello")
	})

	kangle.FlushDiskCache()
	common.Get("/static/index.id?dc=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.Assert("last-modified", len(resp.Header.Get("Last-Modified")) > 0)
	})
	common.Get("/etag?disk_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", resp == nil || strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(resp.Header.Get("Etag"), "hello")
	})

}
func check_cache_etag() {
	common.Get("/dynamic_etag", map[string]string{"x-etag": "1", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "1")
		common.AssertSame(resp.Header.Get("etag"), "1")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/dynamic_etag", map[string]string{"x-etag": "2", "If-None-Match": "2", "Pragma": "no-cache", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(resp.StatusCode, 304)
	})
	common.Get("/dynamic_etag", map[string]string{"x-etag": "2", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "2")
		common.AssertSame(resp.Header.Get("etag"), "2")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
}
func check_cache_etag_last_modified() {
	common.Get("/etag_last_modified", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "ok")
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})

	common.Get("/etag_last_modified", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "ok")
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

	common.Get("/etag_last_modified", map[string]string{"If-None-Match": etag, "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 304)
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Get("/etag_last_modified", map[string]string{"If-Modified-Since": last_modified, "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 304)
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	//wrong If-None-Match,right If-Modified-Since
	common.Get("/etag_last_modified", map[string]string{"If-None-Match": "\"wrong_etag\"", "If-Modified-Since": last_modified, "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	//right If-None-Match,wrong If-Modified-Since
	//规则是以if-none-match为主.
	common.Get("/etag_last_modified", map[string]string{"If-None-Match": etag, "If-Modified-Since": "Thu, 14 Oct 2020 02:45:05 GMT", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 304)
		common.AssertSame(resp.Header.Get("etag"), etag)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

}
func check_not_get_cache() {

	common.CreateRange(1)
	common.Get("/range?not_get", nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Post("/range?not_get", map[string]string{"If-None-Match": common.RangeMd5}, "", func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 412)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Post("/range?not_get", nil, "", func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 405)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Head("/range?not_get", nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

	common.Head("/range?not_get", map[string]string{"Accept-Encoding": "gzip", "If-None-Match": "\"wrong_etag\""}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Head("/range?not_get", map[string]string{"Accept-Encoding": "gzip", "If-None-Match": common.RangeMd5}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 304)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

	common.CreateRange(1024)
	common.Get("/range?not_get2", map[string]string{"Accept-Encoding": "gzip", "Range": "bytes=1-2"}, func(resp *http.Response, err error) {
		//缓存未过期。
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(resp.StatusCode, 206)
	})
	common.Post("/range?not_get2", map[string]string{"Accept-Encoding": "gzip", "If-None-Match": common.RangeMd5}, "", func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 412)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Head("/range?not_get2", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

	common.Head("/range?not_get2", map[string]string{"Accept-Encoding": "gzip", "If-None-Match": "\"wrong_etag\""}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Head("/range?not_get2", map[string]string{"Accept-Encoding": "gzip", "If-None-Match": common.RangeMd5}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 304)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
}
