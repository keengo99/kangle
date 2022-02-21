package base_suite

import (
	"fmt"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/config"
)

func HandleBrokenCache(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	if if_none_match == "" {
		request_count++
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
	request_count++
	w.WriteHeader(304)
	w.Header().Add("Server", TEST_SERVER_NAME)
	return
}
func check_broken_no_cache() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	//带content-length
	common.Get("/broken_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/broken_cache", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	//分块
	common.Get("/broken_cache?chunk=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/broken_cache?chunk=1", nil, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
}
