package access_suite

import (
	"net/http"
	"test_server/common"
	"test_server/config"
)

func handle_header(w http.ResponseWriter, r *http.Request) {
	h := r.FormValue("h")
	header := r.Header.Get("x-header")
	common.AssertSame(h, header)
	w.WriteHeader(200)
	w.Write([]byte("ok"))
}
func check_header() {
	common.Getx("/access/header", config.GetLocalhost("access"), map[string]string{"x-header": "a"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	common.Getx("/access/header?h=b", config.GetLocalhost("access"), map[string]string{"x-header": "b"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})

	common.Getx("/access/header?h=eedee", config.GetLocalhost("access"), map[string]string{"x-header": "eecee"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
}
