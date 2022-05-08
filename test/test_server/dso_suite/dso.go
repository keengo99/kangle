package dso_suite

import (
	"net/http"
	"test_server/common"
)

func check_upstream() {
	common.Getx("http://dso.test:9999/test_dso_upstream", "127.0.0.1", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "test_upstream_ok")
	})
}
func check_filter() {
	common.Getx("http://dso.test:9999/test_dso_filter", "127.0.0.1", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "t*st_upstr*am_ok")
	})
}
