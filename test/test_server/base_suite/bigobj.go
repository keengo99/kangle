package base_suite

import (
	"fmt"
	"net/http"
	"strings"
	"test_server/common"
)

func check_change_first_hit() {
	check_ranges([]RequestRange{
		{0, 8192, nil, func(resp *http.Response, err error) {
			common.Assert("range-status-code", resp.StatusCode == 206)
			createRange(1024)
		}},
		{0, 8192 * 2, nil,
			func(resp *http.Response, err error) {
				common.Assert("change_first_hit-status-code", resp.StatusCode == 200)
				common.Assert("x-big-object-change-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
			}},
	}, false)
}
func check_simple_range() {
	check_ranges([]RequestRange{
		{4095, 4900, nil, func(resp *http.Response, err error) {
		}},
	}, false)
}
func check_change_first_miss() {
	check_ranges([]RequestRange{
		{8190, 8192, nil, func(resp *http.Response, err error) {
			createRange(1024)
		}},
		{0, 8193, nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 200)
				common.AssertContain(resp.Header.Get("X-Cache"), "MISS")
			}},
	}, false)
}
func check_nochange_client_if_range() {
	check_ranges_with_header([]RequestRangeHeader{
		{0, 16 * 1024,
			map[string]string{"Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.Assert("check_nochange_client_if_range-status-code", resp.StatusCode == 206)
			}},
		//*
		{1024, 2048,
			map[string]string{"If-Range": "wrong-test", "Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.AssertSame(resp.StatusCode, 200)
			}},
		//*/
		//*
		{1024, 2048,
			map[string]string{"If-Range": range_md5, "Accept-Encoding": "gzip"},
			nil,
			func(resp *http.Response, err error) {
				common.Assert("check_nochange_client_if_range-right-status-code", resp.StatusCode == 206)
			}},
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
			func(from, to, c int, r *http.Request) {
				common.Assert("range-request", from > 0)
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
			func(from, to, c int, r *http.Request) {
				fmt.Printf("from=[%v],to=[%d]\n", from, to)
				common.Assert("range-request", from > 0)
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
	}, false)
}
func check_sbo_not_enough_bug() {
	///*
	check_ranges([]RequestRange{
		{276409, 1, nil,
			func(resp *http.Response, err error) {
				common.Assert("nochange_first_hit-status_code", resp.StatusCode == 206)
			}},
	}, false)

}

func check_nochange_middle_hit() {
	request_count = 0

	check_ranges([]RequestRange{
		{262044, 102400, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss1", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, 8190, nil, func(resp *http.Response, err error) {
			common.Assert("x-big-object-miss2", strings.Contains(resp.Header.Get("X-Cache"), "MISS"))
		}},
		{0, -1, func(from, to, c int, r *http.Request) {
			//fmt.Printf("from=[%d],to=[%d] c=[%d]\n", from, to, c)
			if c == 0 {
				common.Assert("range-request-middle", from >= 0 && to != -1)
			} else if c == 1 {
				common.Assert("range-request-middle", from > 0 && to == -1)
			} else {
				common.Assert("range-request-middle", false)
			}
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
func check_if_range_forward() {
	check_ranges_with_header([]RequestRangeHeader{
		{0, 1024, map[string]string{"If-Range": range_md5}, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
		}},
	})
}
func check_last_range() {
	/*
		//do not support now.
		check_ranges([]RequestRange{
			{-1, 512, nil, func(resp *http.Response, err error) {
				AssertSame(resp.StatusCode, 206)
				AssertContain(resp.Header.Get("X-Cache"), "MISS ")
			}},
			{-1, 128, nil, func(resp *http.Response, err error) {
				AssertSame(resp.StatusCode, 206)
				AssertContain(resp.Header.Get("X-Cache"), "HIT ")
			}},
		})
	*/
	check_ranges([]RequestRange{
		{range_size - 4096, -1, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
		}},
		{range_size - 512, -1, nil, func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 206)
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		}},
	}, false)
}
