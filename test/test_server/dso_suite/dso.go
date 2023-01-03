package dso_suite

import (
	"net/http"
	"test_server/common"
)

func check_upstream() {
	common.Getx("/test_dso_upstream", "dso.localtest.me", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "test_upstream_ok")
	})
}
func check_filter() {
	common.Getx("/test_dso_filter", "dso.localtest.me", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "t*st_upstr*am_ok")
	})
}
func check_before_cache_chunk() {
	common.Getx("/dso/before_cache_chunk", "dso.localtest.me", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
	})
}
