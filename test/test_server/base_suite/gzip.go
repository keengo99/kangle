package base_suite

import (
	"test_server/common"
	"test_server/config"

	//"fmt"
	"net/http"
	"strings"
	"test_server/kangle"
)

func check_gzip() {
	common.Get("/gzip", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		//Assert("gzip-first-response", read(resp) == "gzip")
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/gzip", map[string]string{"Accept-Encoding": "none"}, func(resp *http.Response, err error) {
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.Assert("gzip-content-encoding2", resp.Header.Get("Content-Encoding") != "gzip")
	})
	common.Get("/gzip", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		//Assert("gzip-second-response", read(resp) == "gzipgzipgzip")
		common.Assert("gzip-content-encoding3", resp.Header.Get("Content-Encoding") == "gzip")
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
}
func check_accept_encoding_with_q() {
	common.Get("/static/index.html?q", map[string]string{"Accept-Encoding": "gzip; q=0"}, func(resp *http.Response, err error) {
		common.Assert("gzip-content-encoding", resp.Header.Get("Content-Encoding") != "gzip")
	})
	common.Get("/static/index.html?q2", map[string]string{"Accept-Encoding": "br; q=0, gzip; q=0.5"}, func(resp *http.Response, err error) {
		common.Assert("gzip-content-encoding", resp.Header.Get("Content-Encoding") == "gzip")
	})
}
func check_br() {
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "br,gzip,deflate"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "br")
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	kangle.CleanAllCache()
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "br,gzip,deflate"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "br")
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
}
func check_id() {
	//检查 content-encoding: identity
	//fmt.Printf("check content-encoding: identity\n")
	common.Get("/static/index.id", map[string]string{"Accept-Encoding": "none"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	common.Get("/static/index.id", map[string]string{"Accept-Encoding": "br,gzip,deflate"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
}

// 测试本地带range的gzip压缩
func check_local_gzip_range() {

	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")

	})

	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "gzip", "Range": "bytes=1-"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.AssertSame(resp.StatusCode, 206)
	})

	common.Get("/static/index.html?aa", map[string]string{"Accept-Encoding": "gzip", "Range": "bytes=1-"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.AssertSame(resp.StatusCode, 200)
	})
}

// 测试代理range的gzip压缩
func check_proxy_gzip_range() {

	check_ranges([]RequestRange{
		{0, -1, nil, nil},
		{0, 8192,
			nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 206)
				common.Assert("gzip", resp.Header.Get("Content-Encoding") == "gzip")
				common.Assert("x-cache", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
			}},
		{1024, 8192,
			nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 206)
				common.Assert("gzip", resp.Header.Get("Content-Encoding") == "gzip")
				common.Assert("x-cache", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
			}},
	}, true)

	check_ranges([]RequestRange{
		{0, 8192, nil, nil},
		{0, -1, nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 200)
				common.Assert("gzip", resp.Header.Get("Content-Encoding") == "gzip")
				common.Assert("x-cache", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART "))
			}},
	}, true)
}
func check_compress() {
	check_gzip()
	check_accept_encoding_with_q()
	if !config.Kangle.DisableBrotli {
		check_br()
	}
	check_id()
	check_proxy_gzip_range()
	check_local_gzip_range()
}
func check_bug() {
	check_id()
}
