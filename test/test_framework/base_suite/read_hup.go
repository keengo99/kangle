package base_suite

import (
	"net/http"
	"test_framework/common"
	"test_framework/config"
)

func HandleReadHup(w http.ResponseWriter, r *http.Request) {
	//r.ParseForm()
	r.ParseForm()
	w.Header().Add("Etag", "etag")
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte("ok"))
}
func check_ssl_pending_readhup() {
	config.Push()
	defer config.Pop()
	config.Cfg.UrlPrefix = config.HttpsUrlPrefix
	config.Cfg.Alpn = 0
	buf := make([]byte, 163840)
	buf[0] = 'a'
	buf[1] = '='
	common.Post("/read_hup", map[string]string{"Content-Type": "application/x-www-form-urlencoded"}, string(buf), func(resp *http.Response, err error) {
		common.Assert("ssl_pending_readhup", resp.StatusCode == 200)
		common.AssertSame(common.Read(resp), "ok")
		common.AssertSame(resp.Header.Get("x-testdso"), "forward-sleep")
	})
}
func check_read_hup() {
	check_ssl_pending_readhup()
}
