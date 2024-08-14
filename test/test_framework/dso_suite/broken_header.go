package dso_suite

import (
	"test_framework/common"
)

func test_broken_static_header() {
	common.Get("/static/index.html?broken_header", map[string]string{"x-broken-header": "1"}, nil)
}
