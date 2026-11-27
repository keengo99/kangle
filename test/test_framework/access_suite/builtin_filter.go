package access_suite

import (
	"net/http"
	"test_framework/common"
	"test_framework/config"
)

func handleBuiltinFilter(w http.ResponseWriter, r *http.Request) {
	_, _ = w.Write([]byte("builtin-filter-ok"))
}

func check_builtin_filter() {
	host := config.GetLocalhost("access")
	common.Getx("/builtin-filter/param?blocked=yes", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusForbidden)
	})
	common.Getx("/builtin-filter/param?allowed=yes", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusOK)
		common.AssertSame(common.Read(resp), "builtin-filter-ok")
	})
	common.Getx("/builtin-filter/count?a=1&b=2", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusForbidden)
	})
	common.Getx("/builtin-filter/count?a=1", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusOK)
		common.AssertSame(common.Read(resp), "builtin-filter-ok")
	})
	common.Getx("/builtin-filter/file", host, nil, func(resp *http.Response, err error) {
		common.AssertSame(err, nil)
		common.AssertSame(resp.StatusCode, http.StatusOK)
		common.AssertSame(common.Read(resp), "builtin-filter-ok")
	})
}
