package base_suite

import (
	"net/http"
	"test_server/common"
)

func check_extworker() {
	common.Post("/a.php2", map[string]string{"Transfer-Encoding": "chunked", "Content-Type": "application/x-www-form-urlencoded"},
		"a=OK",
		func(resp *http.Response, err error) {
			common.AssertSame(common.Read(resp), "OK")
		})
}
