package base_suite

import (
	"net/http"
	"test_framework/common"
)

func check_chunk_upstream_keepalive() {
	var firstConn string
	common.Get("/chunk_keepalive?first", nil, func(resp *http.Response, err error) {
		common.Assert("first chunked upstream request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		firstConn = resp.Header.Get("X-Upstream-Conn")
	})
	// A completely decoded chunked response must leave the upstream
	// connection reusable instead of closing it after every request.
	common.Get("/chunk_keepalive?second", nil, func(resp *http.Response, err error) {
		common.Assert("second chunked upstream request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("chunked upstream connection reused", firstConn != "" && resp.Header.Get("X-Upstream-Conn") == firstConn)
	})
	common.Get("/chunk_keepalive?cl", nil, func(resp *http.Response, err error) {
		common.Assert("chunked with content-length", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
	})
	common.Get("/chunk_keepalive?cl0", nil, func(resp *http.Response, err error) {
		common.Assert("chunked with zero content-length", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
	})
}

func check_http10_upstream_keepalive() {
	var firstConn string
	common.Get("/chunk_keepalive?http10-first", nil, func(resp *http.Response, err error) {
		common.Assert("first HTTP/1.0 upstream request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		firstConn = resp.Header.Get("X-Upstream-Conn")
	})
	common.Get("/chunk_keepalive?http10-second", nil, func(resp *http.Response, err error) {
		common.Assert("second HTTP/1.0 upstream request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("HTTP/1.0 upstream connection not reused", firstConn != "" && resp.Header.Get("X-Upstream-Conn") != firstConn)
	})

	common.Get("/chunk_keepalive?http10ka-first", nil, func(resp *http.Response, err error) {
		common.Assert("first HTTP/1.0 keep-alive request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		firstConn = resp.Header.Get("X-Upstream-Conn")
	})
	common.Get("/chunk_keepalive?http10ka-second", nil, func(resp *http.Response, err error) {
		common.Assert("second HTTP/1.0 keep-alive request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("HTTP/1.0 keep-alive upstream connection reused", firstConn != "" && resp.Header.Get("X-Upstream-Conn") == firstConn)
	})
}
