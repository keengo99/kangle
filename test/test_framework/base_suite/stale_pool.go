package base_suite

import (
	"net/http"
	"test_framework/common"
)

func check_stale_pool_retry() {
	common.Get("/stale_pool?first", nil, func(resp *http.Response, err error) {
		common.Assert("first stale-pool request", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "ok")
	})

	// The test origin reads this request from the pooled connection and then
	// closes it without sending a response.  Kangle must retry the safe request
	// on a fresh connection instead of returning 504.
	common.Get("/stale_pool?second", nil, func(resp *http.Response, err error) {
		common.Assert("stale pooled connection retry", err == nil && resp.StatusCode == http.StatusOK)
		common.AssertSame(common.Read(resp), "ok")
	})
}
