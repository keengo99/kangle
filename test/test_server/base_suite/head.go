package base_suite

import (
	"net/http"
	"strings"
	"test_server/common"
	"test_server/config"
)

func test_head_method() {
	config.Push()
	defer config.Pop()
	config.UseHttp2Client()
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, nil)
	common.Head("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, nil)
	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	common.Get("/static/index.html", map[string]string{"Range": "bytes=102400-", "Accept-Encoding": "identity"},
		func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 416)
		})
}
