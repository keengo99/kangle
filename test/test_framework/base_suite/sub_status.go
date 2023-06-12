package base_suite

import (
	"net/http"
	"test_framework/common"
)

func test_stub_status() {
	common.Get("/stub_status", nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertContain(common.Read(resp), "Reading")
	})
}
