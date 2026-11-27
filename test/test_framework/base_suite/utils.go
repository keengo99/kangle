package base_suite

import (
	"fmt"
	"net/http"
	"test_framework/common"
	"test_framework/kangle"
	"test_framework/server"
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

var rangeScenarioSequence uint64
var rangeScenario string

func beginRangeScenario() func() {
	previous := rangeScenario
	rangeScenarioSequence++
	rangeScenario = fmt.Sprintf("case%d", rangeScenarioSequence)
	return func() { rangeScenario = previous }
}

func rangePath(gzip bool) string {
	path := "/range"
	separator := "?"
	if rangeScenario != "" {
		path += "/" + rangeScenario
	}
	if gzip {
		path += separator + "g=1"
	}
	return path
}

func rangeTestHeader(header map[string]string, checker common.RangeCallBackCheck) map[string]string {
	result := make(map[string]string, len(header)+1)
	for name, value := range header {
		result[name] = value
	}
	result["X-Kangle-Test-ID"] = common.RegisterRangeChecker(checker)
	return result
}

func check_range_all_with_header(gzip bool, header map[string]string, sc common.RangeCallBackCheck, cc common.ClientCheckBack, requireComplete bool) {
	header = rangeTestHeader(header, sc)
	path := rangePath(gzip)
	common.Get(path, header, func(resp *http.Response, err error) {
		if err != nil {
			fmt.Printf("get error [%s]\n", err.Error())
		}
		if !common.Assert("range response must not be nil", resp != nil) {
			return
		}
		expectedLength := common.RangeSize
		if gzip {
			expectedLength = common.GzRangeSize
		}
		common.Assert(fmt.Sprintf("range content-length: expected=%d actual=%d status=%d x-cache=%q",
			expectedLength, resp.ContentLength, resp.StatusCode, resp.Header.Get("X-Cache")),
			expectedLength == int(resp.ContentLength))
		buf := common.ReadRange(0, -1, gzip)
		common.AssertRespComplete(buf, resp, requireComplete)
		//common.Assert("range-md5", range_md5 == common.Md5Response(resp, true))
		if cc != nil {
			cc(resp, err)
		}
	})
	common.Assert("origin requests must become idle", server.WaitIdle(300*time.Millisecond, 5*time.Second))
}
func check_ranges(rrs []RequestRange, gzip bool, allowIncompleteLast ...bool) {
	defer beginRangeScenario()()
	kangle.CleanAllCache()
	for i, rr := range rrs {
		allowIncomplete := len(allowIncompleteLast) > 0 && allowIncompleteLast[0] && i == len(rrs)-1
		check_range(rr.from, rr.length, gzip, rr.sc, rr.cc, allowIncomplete)
	}
}
func check_ranges_with_header(rrs []RequestRangeHeader) {
	defer beginRangeScenario()()
	kangle.CleanAllCache()
	for _, rr := range rrs {
		check_range_with_header(false, rr.from, rr.length, rr.header, rr.sc, rr.cc)
	}
}
func check_range(from int, length int, gzip bool, sc common.RangeCallBackCheck, cc common.ClientCheckBack, allowIncomplete ...bool) {
	//fmt.Printf("from=[%d] length=[%d]\n", from, length)
	check_range_with_header(gzip,
		from,
		length,
		map[string]string{"Accept-Encoding": "gzip"},
		sc,
		cc,
		allowIncomplete...)
}
func check_range_with_header(gzip bool, from int, length int, header map[string]string, sc common.RangeCallBackCheck, cc common.ClientCheckBack, allowIncomplete ...bool) {
	common.RequestCount = 0
	requireComplete := len(allowIncomplete) == 0 || !allowIncomplete[0]

	if from == 0 && length == -1 {
		check_range_all_with_header(gzip, header, sc, cc, requireComplete)
		return
	}
	if header == nil {
		header = make(map[string]string)
	}
	header = rangeTestHeader(header, sc)
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
	path := rangePath(gzip)
	check_size := common.RangeSize
	if gzip {
		check_size = common.GzRangeSize
	}
	common.Get(path, header, func(resp *http.Response, err error) {
		buf := common.ReadRange(0, -1, gzip)
		buf_range := buf[from:]
		if resp.StatusCode == 206 {
			//checkRespRange(resp,from,to)
			//common.Assert(fmt.Sprintf("range-md5-%d-%d", from, to), common.Md5Response(resp, false) == md5_range)
			common.AssertRespComplete(buf_range, resp, requireComplete)
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
