package base_suite

import (
	"bufio"
	"fmt"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/kangle"
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

func check_range_all(sc common.RangeCallBackCheck, cc common.ClientCheckBack) {
	check_range_all_with_header(false, map[string]string{"Accept-Encoding": "gzip"}, sc, cc)
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
	common.Get(path, header, func(resp *http.Response, err error) {
		if err != nil {
			fmt.Printf("get error [%s]\n", err.Error())
		}
		if gzip {
			common.Assert("range", common.GzRangeSize == int(resp.ContentLength))
		} else {
			common.Assert("range", common.RangeSize == int(resp.ContentLength))
		}
		common.Assert("range-md5", common.RangeMd5 == common.Md5Response(resp, true))
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
		to = common.RangeSize - 1
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
	common.Get(path, header, func(resp *http.Response, err error) {
		if resp.StatusCode == 206 {
			//checkRespRange(resp,from,to)
			common.Assert(fmt.Sprintf("range-md5-%d-%d", from, to), common.Md5Response(resp, false) == common.Md5File(from, to, gzip))
			common.Assert("range-content-length", length == int(resp.ContentLength))
		} else {
			common.Assert("range-size-200", check_size == int(resp.ContentLength))
			common.Assert("range-md5-200", common.RangeMd5 == common.Md5Response(resp, true))
		}
		if cc != nil {
			cc(resp, err)
		}
	})
}
func readHttpHeader(reader *bufio.Reader, read_body bool) (map[string]string, string, error) {
	headers := make(map[string]string)
	request_line := true
	for {
		line, _, err := reader.ReadLine()
		if err != nil {
			return headers, "", err
		}
		if len(line) == 0 {
			break
		}
		splitChar := ":"
		if request_line {
			splitChar = " "
		}
		request_line = false
		kvs := strings.SplitN(string(line), splitChar, 2)
		headers[strings.ToLower(kvs[0])] = strings.TrimSpace(kvs[1])
	}
	if !read_body {
		return headers, "", nil
	}
	line, _, err := reader.ReadLine()
	return headers, string(line), err
}
