package base_suite

import (
	"net/http"
	"strings"
	"test_framework/common"
)

func check_change_first_hit() {
	check_ranges([]RequestRange{
		{0, 8192, nil, func(resp *http.Response, err error) {
			common.Assert("range-status-code", resp.StatusCode == 206)
			common.CreateRange(1024)
		}},
		{0, 8192 * 2, nil,
			func(resp *http.Response, err error) {
				common.Assert("change_first_hit-status-code", resp.StatusCode == 200)
				common.Assert("x-big-object-change-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
			}},
		{0, -1, nil, nil},
	}, false)
}
func check_simple_range() {
	check_ranges([]RequestRange{
		{4095, 4900, nil, func(resp *http.Response, err error) {
		}},
		{0, -1, nil, nil},
	}, false)
}
func check_change_first_miss() {
	check_ranges([]RequestRange{
		{8190, 8192, nil, func(resp *http.Response, err error) {
			common.CreateRange(1024)
		}},
		{0, 8193, nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 200)
				common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
			}},
		{0, -1, nil, nil},
	}, false)
}
func check_nochange_if_range() {
	check_ranges_with_header([]RequestRangeHeader{
		{0, 16 * 1024,
			map[string]string{"Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.Assert("check_nochange_client_if_range-status-code", resp.StatusCode == 206)
			}},
		//*
		{1024, 2048,
			map[string]string{"If-Range": "\"wrong-test\"", "Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 200)
			}},
		//*/
		//*
		{1024, 2048,
			map[string]string{"If-Range": common.RangeMd5, "Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 206)
			}},
		{0, -1, nil, nil, nil},
		//*/
	})
}
func check_nochange_first_miss() {
	//命中后面
	check_ranges([]RequestRange{
		{8190, 4096, nil, nil},

		{0, 8193, nil,
			func(resp *http.Response, err error) {
				common.Assert("nochange_first_miss-status-code", resp.StatusCode == 206)
				common.Assert("x-big-object-hit-part1", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART "))
			}},
		//*
		{0, -1,
			func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
				common.Assert("range-request", from > 0)
				return true
			},
			func(resp *http.Response, err error) {
				common.Assert("x-big-object-hit-part2", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART "))
			}},
		//*/
	}, false)
}
func check_nochange_first_part() {
	check_ranges([]RequestRange{
		{0, 8192 + 333, nil, nil},
		{0, -1,
			func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
				//fmt.Printf("from=[%v],to=[%d]\n", from, to)
				common.Assert("range-request", from > 0)
				return true
			},
			func(resp *http.Response, err error) {
				common.Assert("x-big-object-hit-part3", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART "))
			}},
	}, false)
}
func check_nochange_first_hit() {
	///*
	check_ranges([]RequestRange{
		{0, 8192, nil, nil},
		{0, 4096, nil,
			func(resp *http.Response, err error) {
				common.Assert("nochange_first_hit-status_code", resp.StatusCode == 206)
				common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
			}},
		{0, -1, nil, nil},
	}, false)
}
func check_sbo_not_enough_bug() {
	///*
	check_ranges([]RequestRange{
		{276409, 1, nil,
			func(resp *http.Response, err error) {
				common.Assert("nochange_first_hit-status_code", resp.StatusCode == 206)
			}},
		{0, -1, nil, nil},
	}, false)

}

func check_nochange_middle_hit() {
	common.RequestCount = 0

	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, 8190, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
			//fmt.Printf("from=[%d],to=[%d] c=[%d]\n", from, to, c)
			if c == 0 {
				common.Assert("range-request-middle", from >= 0 && to != -1)
			} else if c == 1 {
				common.Assert("range-request-middle", from > 0 && to == -1)
			} else {
				common.Assert("range-request-middle", false)
			}
			return true
		}, func(resp *http.Response, err error) {
			common.Assert("x-big-object-hit-part4", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART"))
		}},
	}, false)
}
func check_big_object() {
	check_ranges_with_header([]RequestRangeHeader{
		{0, -1, nil, nil, nil},
		{0, -1, nil, nil, func(resp *http.Response, err error) {
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		}},
	})
}

func check_last_range() {

	check_ranges([]RequestRange{
		{-1, 512, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		}},
		{-1, 128, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		}},
		{0, -1, nil, nil},
	}, false)

	check_ranges([]RequestRange{
		{common.RangeSize - 4096, -1, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		}},
		{common.RangeSize - 512, -1, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		}},
		{0, -1, nil, nil},
	}, false)
}
func check_bigobj_upstream_error() {
	//multi miss change
	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
			if c == 1 {
				common.SkipCheckRespComplete = true
				cn, _ := w.(http.Hijacker)
				c, _, err := cn.Hijack()
				if err == nil {
					c.Close()
					return false
				}
			}
			return true
		}, nil},
	}, false)
}
func check_multi_miss() {
	//multi miss nochange
	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{100000, 8190, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
			//fmt.Printf("from=[%d],to=[%d] c=[%d]\n", from, to, c)
			if c == 0 {
				common.Assert("range-request-middle", from >= 0 && to != -1)
			} else if c == 1 {
				common.Assert("range-request-middle", from > 0 && to != -1)
			} else if c == 2 {
				common.Assert("range-request-middle", from > 0 && to == -1)
			} else {
				common.Assert("range-request-middle", false)
			}
			return true
		}, func(resp *http.Response, err error) {
			common.Assert("x-big-object-hit-part4", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART"))
		}},
	}, false)

	//multi miss change
	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{100000, 8190, func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool {
			common.CreateRange(1024)
			return true
		}, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
			if c == 0 {
				//common.CreateRange(1024)
			} else if c == 1 {
				common.Assert("range-request-middle", from > 0 && to != -1)
			} else {
				common.Assert("range-request-middle", false)
			}
			return true
		}, func(resp *http.Response, err error) {
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS")

		}},
	}, false)

	//multi miss change
	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{100000, 8190, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request, w http.ResponseWriter) bool {
			//fmt.Printf("from=[%d],to=[%d] c=[%d]\n", from, to, c)
			if c == 0 {
				common.CreateRange(1024)
				common.SkipCheckRespComplete = true
			}
			return true
		}, func(resp *http.Response, err error) {
			common.Assert("x-big-object-hit-part4", strings.Contains(resp.Header.Get("X-Cache"), "HIT-PART"))
		}},
	}, false)
}
func check_weak_etag_range() {
	common.CreateRange(1024)
	common.Get("/range?weak", map[string]string{"x-weak-etag": "1", "Range": "bytes=0-4096", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 206)
		common.AssertContain(resp.Header.Get("etag"), "W/")
	})

	common.Get("/range?weak", map[string]string{"x-weak-etag": "1", "Range": "bytes=0-4096"}, func(resp *http.Response, err error) {
		//fmt.Printf("etag=" + resp.Header.Get("etag"))
		common.AssertContain(resp.Header.Get("etag"), "W/")
		common.AssertSame(resp.StatusCode, 206)
		common.Assert("x-big-object-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT"))
	})

	common.CreateRange(1024)
	common.Get("/range?weak", map[string]string{"x-weak-etag": "1", "Accept-Encoding": "gzip"}, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("etag"), "W/")
		//fmt.Printf("etag=" + resp.Header.Get("etag"))
		common.Assert("x-big-object-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		common.AssertSame(common.RangeMd5, common.Md5Response(resp, true))
	})
}

/**
* 客户发送If-Range和自已的不一样
 */
func check_bigobj_client_if_range() {
	/* client is right if-range */
	check_ranges_with_header([]RequestRangeHeader{
		{2048, 8192, map[string]string{"Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
			common.CreateRange(1024)
			//fmt.Printf("md5=%v\n", common.RangeMd5)
		}},
	})
	check_range_with_header(false, 0, 8192, map[string]string{"If-Range": common.RangeMd5, "Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 206)
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
	})
	check_range_with_header(false, 0, -1, map[string]string{"Accept-Encoding": "gzip"}, func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool {
		return true
	}, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT-PART")
	})

	/* client is wrong if-range cann't satisfy*/
	check_ranges_with_header([]RequestRangeHeader{
		{2048, 8192, map[string]string{"Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
			common.CreateRange(1024)
		}},
	})
	check_range_with_header(false, 0, 8192, map[string]string{"If-Range": "\"wrong_etag\"", "Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
	})
	check_range_with_header(false, 0, -1, map[string]string{"Accept-Encoding": "gzip"}, func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool {
		return true
	}, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT")
	})

	/* client is wrong if-range can satisfy */
	check_ranges_with_header([]RequestRangeHeader{
		{2048, 8192, map[string]string{"Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
			common.CreateRange(1024)
		}},
	})
	check_range_with_header(false, 2048, 8192, map[string]string{"If-Range": "\"wrong_etag\"", "Accept-Encoding": "gzip"}, nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
	})
	check_range_with_header(false, 0, -1, map[string]string{"Accept-Encoding": "gzip"}, func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool {
		return true
	}, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("X-Cache"), "HIT")
	})
}
