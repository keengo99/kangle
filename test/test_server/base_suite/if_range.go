package base_suite

import (
	"net/http"
	"strings"
	"test_server/common"
	"time"
)

func check_if_range_forward() {
	check_ranges_with_header([]RequestRangeHeader{
		{0, 1024, map[string]string{"If-Range": common.RangeMd5}, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
		}},
	})
}
func check_if_range_local() {
	var last_modified string
	var wrong_modified string
	common.Get("/static/index.id?if_range", map[string]string{"Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.Assert("gzip-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		last_modified = resp.Header.Get("Last-Modified")
		t, _ := time.Parse(time.RFC1123, last_modified)
		t = t.AddDate(0, 0, -1)
		wrong_modified = t.Format(time.RFC1123)
	})
	//from cache
	common.Get("/static/index.id?if_range", map[string]string{"Accept-Encoding": "gzip", "If-Range": last_modified, "Range": "bytes=1-2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 206)
	})
	common.Get("/static/index.id?if_range", map[string]string{"Accept-Encoding": "gzip", "If-Range": wrong_modified, "Range": "bytes=1-2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	//from no-cache
	common.Get("/static/index.id?if_range2", map[string]string{"Accept-Encoding": "gzip", "If-Range": last_modified, "Range": "bytes=1-2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 206)
	})
	common.Get("/static/index.id?if_range3", map[string]string{"Accept-Encoding": "gzip", "If-Range": wrong_modified, "Range": "bytes=1-2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
}
