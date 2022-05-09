package cmd_suite

import (
	"net/http"
	"test_server/common"
)

func check_common(path string) {
	common.Getx(path, "127.0.0.1", nil, func(resp *http.Response, err error) {
		common.AssertContain(common.Read(resp), "hello")
	})
}
func check_mp_fastcgi() {
	check_common("http://cmd.test:9999/test.fastcgi_mp")
}
func check_sp_fastcgi() {
	check_common("http://cmd.test:9999/test.fastcgi_sp")
}
func check_mp_http() {
	check_common("http://cmd.test:9999/test.http_mp")
}
func check_sp_http() {
	check_common("http://cmd.test:9999/test.http_sp")
}
func check_fastcgi() {
	check_common("http://cmd.test:9999/test.fastcgi")
}
