package base_suite

import (
	"fmt"
	"net/http"
	"test_framework/common"
	"test_framework/kangle"
	"time"
)

type RequestRange struct {
	from, length int
	sc           common.RangeCallBackCheck
	cc           common.ClientCheckBack
}
type RequestRangeHeader struct {
	from, length int
	header       map[string]string
	sc           common.RangeCallBackCheck
	cc           common.ClientCheckBack
}

func check_range_all_with_header(gzip bool, header map[string]string, sc common.RangeCallBackCheck, cc common.ClientCheckBack) {
	common.RangeChecker = sc
	defer func() {
		common.RangeChecker = nil
	}()
	path := "/range"
	if gzip {
		path += "?g=1"
	}
	buf := common.ReadRange(0, -1, gzip)
	common.Get(path, header, func(resp *http.Response, err error) {
		if err != nil {
			fmt.Printf("get error [%s]\n", err.Error())
		}
		if gzip {
			common.Assert("range", common.GzRangeSize == int(resp.ContentLength))
		} else {
			common.Assert("range", common.RangeSize == int(resp.ContentLength))
		}
		common.AssertResp(buf, resp)
		//common.Assert("range-md5", range_md5 == common.Md5Response(resp, true))
		if cc != nil {
			cc(resp, err)
		}
	})
}
func check_ranges(rrs []RequestRange, gzip bool) {
	kangle.CleanAllCache()
	for _, rr := range rrs {
		check_range(rr.from, rr.length, gzip, rr.sc, rr.cc)
		time.Sleep(50 * time.Millisecond)
	}
}
func check_ranges_with_header(rrs []RequestRangeHeader) {
	kangle.CleanAllCache()
	for _, rr := range rrs {
		check_range_with_header(false, rr.from, rr.length, rr.header, rr.sc, rr.cc)
	}
}
func check_range(from int, length int, gzip bool, sc common.RangeCallBackCheck, cc common.ClientCheckBack) {
	//fmt.Printf("from=[%d] length=[%d]\n", from, length)
	check_range_with_header(gzip,
		from,
		length,
		map[string]string{"Accept-Encoding": "gzip"},
		sc,
		cc)
}
func check_range_with_header(gzip bool, from int, length int, header map[string]string, sc common.RangeCallBackCheck, cc common.ClientCheckBack) {
	common.RequestCount = 0
	common.SkipCheckRespComplete = false

	if from == 0 && length == -1 {
		check_range_all_with_header(gzip, header, sc, cc)
		return
	}
	if header == nil {
		header = make(map[string]string)
	}
	common.RangeChecker = sc
	defer func() {
		common.RangeChecker = nil
	}()
	var range_header string
	var to int
	if length == -1 {
		range_header = fmt.Sprintf("bytes=%d-", from)
		to = common.RangeSize - 1
		length = to - from + 1
	} else if from == -1 {
		range_header = fmt.Sprintf("bytes=-%d", length)
		from = common.RangeSize - length
	} else {
		to = from + length - 1
		range_header = fmt.Sprintf("bytes=%d-%d", from, to)
	}
	//fmt.Printf("range_header=[%s]\n", range_header)
	header["Range"] = range_header
	path := "/range"
	check_size := common.RangeSize
	if gzip {
		path += "?g=1"
		check_size = common.GzRangeSize
	}
	buf := common.ReadRange(0, -1, gzip)
	buf_range := buf[from:]
	common.Get(path, header, func(resp *http.Response, err error) {
		if resp.StatusCode == 206 {
			//checkRespRange(resp,from,to)
			//common.Assert(fmt.Sprintf("range-md5-%d-%d", from, to), common.Md5Response(resp, false) == md5_range)
			common.AssertResp(buf_range, resp)
			common.Assert("range-content-length", length == int(resp.ContentLength))
		} else {
			common.Assert("range-size-200", check_size == int(resp.ContentLength))
			common.AssertResp(buf, resp)
		}
		if cc != nil {
			cc(resp, err)
		}
	})
}
