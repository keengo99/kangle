package base_suite

import (
	"test_framework/common"
	"test_framework/config"

	//"fmt"
	"net/http"
	"strings"
	"test_framework/kangle"
)

func handle_chunk_compress(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("X-Gzip", "on")
	w.Header().Add("Cache-Control", "no-cache, no-store")
	w.Header().Add("Server", TEST_SERVER_NAME)
	f, _ := w.(http.Flusher)
	f.Flush()
	w.Write([]byte(r.FormValue("body")))
}
func check_chunk_compress() {
	common.Get("/chunk_compress?body=helloworld", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.AssertSame(common.ReadGzip(resp), "helloworld")
	})
	common.Get("/chunk_compress?body=", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
		common.AssertSame(common.ReadGzip(resp), "")
	})
}
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
		common.AssertSame(resp.StatusCode, 200)
		common.Assert("gzip-content-encoding", resp.Header.Get("Content-Encoding") != "gzip")
	})
	common.Get("/static/index.html?q2", map[string]string{"Accept-Encoding": "br; q=0, gzip; q=0.5"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
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
		{0, -1, func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool {
			common.AssertSame(from, 8192)
			return true
		},
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
