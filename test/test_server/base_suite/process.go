package base_suite

import (
	"fmt"
	"test_server/common"
	"test_server/config"

	"net/http"
	"strings"
)

func check_bigobj_md5() {

	common.CreateRange(1024)
	common.Get("/range", map[string]string{"pragma": "no-cache"}, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		common.AssertSame(common.RangeMd5, common.Md5Response(resp, true))
	})
	common.Get("/range", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.RangeMd5, common.Md5Response(resp, true))
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
	})
}
func check_dynamic_content() {
	common.RequestCount = 0
	for i := 0; i < 2; i++ {
		common.Get("/dynamic", map[string]string{"pragma": "no-cache"}, func(resp *http.Response, err error) {
			common.AssertSame(common.Read(resp), last_dynamic_content)
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		})
		common.Get("/dynamic", nil, func(resp *http.Response, err error) {
			common.AssertSame(common.Read(resp), last_dynamic_content)
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		})
	}
}
func check_etag() {
	common.RequestCount = 0

	common.Get("/etag", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	//*
	common.Get("/etag", map[string]string{"pragma": "no-cache"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Assert("progma-no-cache", common.RequestCount == 2)
	common.Get("/etag", map[string]string{"if-none-match": "hello"}, func(resp *http.Response, err error) {
		common.Assert("etag-304-response", resp.StatusCode == 304)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Assert("cache-no-verify", common.RequestCount == 2)

	common.Get("/etag", map[string]string{"if-none-match": "*"}, func(resp *http.Response, err error) {
		common.Assert("if-none-match: *", resp.StatusCode == 304)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Assert("cache-no-verify", common.RequestCount == 2)

	/**
	* 304 should only be returned in response for GET and HEAD requests,
	* and all cache-related response headers have to be there.
	* For all other types of request your server needs to be answering 412 (Precondition failed).
	 */
	common.Post("/etag", map[string]string{"if-none-match": "hello"}, "", func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(resp.StatusCode, 412)
	})
	common.Assert("cache-no-verify", common.RequestCount == 2)

	/* multi etag check */
	common.Get("/etag", map[string]string{"if-none-match": "\"lll\" ,hello, abc , \"sss\""}, func(resp *http.Response, err error) {
		common.Assert("etag-304-response", resp.StatusCode == 304)
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Assert("cache-no-verify", common.RequestCount == 2)

	//test h2c
	common.RequestCount = 0
	common.Get("/upstream/h2c/etag", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/upstream/h2c/etag", map[string]string{"pragma": "no-cache"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
}
func check_miss_status_string() {
	if config.Cfg.UpstreamHttp2 {
		//本测试不支持上游为http2协议
		fmt.Printf("skip...\n")
		return
	}
	common.Get("/miss_status_string", nil, func(resp *http.Response, err error) {
		common.Assert("miss_status_string", common.Read(resp) == "ok")
		common.Assert("miss_status_string", resp.StatusCode == 200)
	})
}
func check_http2https() {
	config.Push()
	defer config.Pop()
	config.Cfg.UrlPrefix = "http://127.0.0.1:9943"
	config.Cfg.Alpn = config.HTTP1

	common.Get("/kangle.status", nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		if err == nil {
			common.AssertSame(resp.StatusCode, 497)
		}
	})
}
func check_port_is_ok(port int, ssl bool) {
	config.Push()
	defer config.Pop()
	config.Cfg.Alpn = config.HTTP1

	url := "http"
	if ssl {
		url = "https"
	}
	url = fmt.Sprintf("%s://127.0.0.1:%d", url, port)
	//fmt.Printf("now check url=[%s] is ok.\n", url)
	config.Cfg.UrlPrefix = url
	common.Get("/kangle.status", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "OK\n")
	})
}
func check_proxy_port() {
	check_port_is_ok(9800, false)
}

func check_bug() {

	/* client is wrong if-range can satisfy */
	check_ranges_with_header([]RequestRangeHeader{
		{2048, 8192, map[string]string{"Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
			common.CreateRange(1024)
		}},
	})
	check_range_with_header(false, 2048, 8192, map[string]string{"If-Range": "\"wrong_etag\"", "Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
	})
}
