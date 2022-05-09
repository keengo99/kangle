package webdav_suite

import (
	"net/http"
	"test_server/common"
)

func check_options() {
	common.Request("OPTIONS", "http://webdav.test:9999/dav", "127.0.0.1", nil, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("DAV"), "1")
	})
}
