package access_suite

import (
	"net/http"
	"test_framework/common"
	"test_framework/config"
)

func handle_auth(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("ok"))
}
func check_http_auth() {
	common.Getx("/auth_load_failed", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 500)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), map[string]string{"AUTH_BASIC": "user,pass2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), map[string]string{"AUTH_BASIC": "user,pass"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
}
