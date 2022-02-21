package filter_suite

import (
	"net/http"
	"test_server/common"
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
	return
}
func check_footer() {

	common.Get("/footer", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hellofooter")
	})
	common.Get("/footer", map[string]string{"Pragma": "no-cache"}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hellofooter")
		common.AssertSame(footer_304_response, true)
	})
}
