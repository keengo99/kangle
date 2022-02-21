package dso_suite

import (
	"net/http"
	"test_server/common"
)

func check_upstream() {
	common.Get("/test_dso_upstream", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "test_upstream_ok")
	})
}
func check_filter() {
	common.Get("/test_dso_filter", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "t*st_upstr*am_ok")
	})
}
