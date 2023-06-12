package access_suite

import (
	"net/http"
	"test_framework/common"
	"test_framework/config"
)

var footer_304_response bool

func handleFooter(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	if if_none_match == "" {
		w.Header().Add("Etag", "hello")
		w.Write([]byte("hello"))
		return
	}
	footer_304_response = true
	common.Assert("if-none-match-1", if_none_match == "hello")
	w.WriteHeader(304)
}
func check_footer() {

	common.Getx("/footer", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hellofooter")
	})
	common.Getx("/footer", config.GetLocalhost("access"), map[string]string{"Pragma": "no-cache"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hellofooter")
		common.AssertSame(footer_304_response, true)
	})
}
