package base_suite

import (
	"fmt"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/kangle"
)

func check_gzip_br_encodings(encodings []string) {
	kangle.CleanAllCache()
	for _, encoding := range encodings {
		common.Get("/gzip_br", map[string]string{"Accept-Encoding": encoding}, func(resp *http.Response, err error) {
			common.Assert(fmt.Sprintf("x-cache-miss-%s", encoding), strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
			if encoding == "identity" {
				encoding = ""
			}
			common.Assert(fmt.Sprintf("x-content-encoding-%s", encoding), resp.Header.Get("Content-Encoding") == encoding)
		})
	}
	////////////////////
	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "gzip,br"}, func(resp *http.Response, err error) {
		//Assert("gzip_br-response", read(resp) == "br")
		common.AssertSame(common.Read(resp), "br")
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		common.AssertSame(resp.Header.Get("Content-Encoding"), "br")
	})
	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "br"}, func(resp *http.Response, err error) {
		//Assert("gzip_br-response", read(resp) == "br")
		common.AssertSame(common.Read(resp), "br")
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		common.AssertSame(resp.Header.Get("Content-Encoding"), "br")
	})

	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		//Assert("gzip_br-response", read(resp) == "br")
		common.AssertSame(common.Read(resp), "gzip")
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		common.AssertSame(resp.Header.Get("Content-Encoding"), "gzip")
	})
	///////////////////

	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		common.Assert("gzip_br-response", common.Read(resp) == "gzip")
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "gzip")
	})
	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "deflate,br"}, func(resp *http.Response, err error) {
		common.Assert("gzip_br-response", common.Read(resp) == "br")
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "br")
	})
	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "deflate,unknow"}, func(resp *http.Response, err error) {
		common.Assert("gzip_br-response", common.Read(resp) == "deflate")
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "deflate")
	})
	common.Get("/gzip_br", map[string]string{"Accept-Encoding": "identity"}, func(resp *http.Response, err error) {
		common.Assert("gzip_br-response", common.Read(resp) == "plain")
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "")
	})
}
func check_encoding_priority() {
	//检查不同顺序请求encoding的优先级,
	//br>gzip>deflate
	check_gzip_br_encodings([]string{"deflate", "br", "identity", "gzip"})
	check_gzip_br_encodings([]string{"gzip", "identity", "deflate", "br"})
	check_gzip_br_encodings([]string{"identity", "gzip", "br", "deflate"})
	check_gzip_br_encodings([]string{"br", "gzip", "identity", "deflate"})
}

//检测br和unknow的encoding
func check_br_unknow_encoding() {
	common.Get("/br", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		common.Assert("br-first-response", common.Read(resp) == "plain")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})

	//check identity encoding
	common.Get("/br", map[string]string{"Accept-Encoding": "identity"}, func(resp *http.Response, err error) {
		common.Assert("br-second-response", common.Read(resp) == "plain")
		common.Assert("x-cache-hit1", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Get("/br", map[string]string{"Accept-Encoding": "br,gzip,deflate"}, func(resp *http.Response, err error) {
		common.Assert("br-nocache-response", common.Read(resp) == "br")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "br")
	})
	for i := 0; i < 2; i++ {
		//unknow的encoding不缓存
		common.Get("/br", map[string]string{"Accept-Encoding": "unknow"}, func(resp *http.Response, err error) {
			common.Assert("unknow-response", common.Read(resp) == "unknow")
			common.Assert("x-cache-unknow-encoding-must-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		})
	}
	//*/

	common.Get("/br", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.Assert("br-second-response", common.Read(resp) == "plain")
		common.Assert("x-cache-hit2", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Get("/br", map[string]string{"Accept-Encoding": "gzip,deflate"}, func(resp *http.Response, err error) {
		common.Assert("br-second-response", common.Read(resp) == "plain")
		common.Assert("x-cache-hit3", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	//未压缩的，只要存在未知的encoding，则不命中
	common.Get("/br", map[string]string{"Accept-Encoding": "gzip,deflate,dd2"}, func(resp *http.Response, err error) {
		common.Assert("br-first-response", common.Read(resp) == "plain")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	//压缩的，缓存命中
	common.Get("/br", map[string]string{"Accept-Encoding": "br"}, func(resp *http.Response, err error) {
		common.Assert("br-second-response", common.Read(resp) == "br")
		common.Assert("x-cache-hit4", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "br")
	})
	/*
		//即使存在未知的encoding，只要任意一个accept-encoding命中即可
		common.Get("/br", map[string]string{"Accept-Encoding": "br,dd2"}, func(resp *http.Response, err error) {
			common.Assert("br-second-response", common.Read(resp) == "br")
			common.Assert("x-cache-hit5", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
			common.Assert("x-content-encoding", resp.Header.Get("Content-Encoding") == "br")
		})
	*/
}
