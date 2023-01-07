package base_suite

import (
	"bufio"
	"compress/gzip"
	"crypto/md5"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/kangle"
	"time"
)

type RequestRange struct {
	from, length int
	sc           rangeCallBackCheck
	cc           common.ClientCheckBack
}
type RequestRangeHeader struct {
	from, length int
	header       map[string]string
	sc           rangeCallBackCheck
	cc           common.ClientCheckBack
}

func check_range_all(sc rangeCallBackCheck, cc common.ClientCheckBack) {
	check_range_all_with_header(false, map[string]string{"Accept-Encoding": "gzip"}, sc, cc)
}
func check_range_all_with_header(gzip bool, header map[string]string, sc rangeCallBackCheck, cc common.ClientCheckBack) {
	rangeChecker = sc
	defer func() {
		rangeChecker = nil
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
			common.Assert("range", gz_range_size == int(resp.ContentLength))
		} else {
			common.Assert("range", range_size == int(resp.ContentLength))
		}
		common.Assert("range-md5", range_md5 == md5Response(resp, true))
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
func check_range(from int, length int, gzip bool, sc rangeCallBackCheck, cc common.ClientCheckBack) {
	//fmt.Printf("from=[%d] length=[%d]\n", from, length)
	check_range_with_header(gzip,
		from,
		length,
		map[string]string{"Accept-Encoding": "gzip"},
		sc,
		cc)
}
func check_range_with_header(gzip bool, from int, length int, header map[string]string, sc rangeCallBackCheck, cc common.ClientCheckBack) {
	request_count = 0
	if from == 0 && length == -1 {
		check_range_all_with_header(gzip, header, sc, cc)
		return
	}
	if header == nil {
		header = make(map[string]string)
	}
	rangeChecker = sc
	defer func() {
		rangeChecker = nil
	}()
	var range_header string
	var to int
	if length == -1 {
		range_header = fmt.Sprintf("bytes=%d-", from)
		to = range_size - 1
		length = to - from + 1
	} else if from == -1 {
		range_header = fmt.Sprintf("bytes=-%d", length)
		to = range_size - 1
		from = range_size - length
	} else {
		to = from + length - 1
		range_header = fmt.Sprintf("bytes=%d-%d", from, to)
	}
	//fmt.Printf("range_header=[%s]\n", range_header)
	header["Range"] = range_header
	path := "/range"
	check_size := range_size
	if gzip {
		path += "?g=1"
		check_size = gz_range_size
	}
	common.Get(path, header, func(resp *http.Response, err error) {
		if resp.StatusCode == 206 {
			//checkRespRange(resp,from,to)
			common.Assert(fmt.Sprintf("range-md5-%d-%d", from, to), md5Response(resp, false) == md5File(from, to, gzip))
			common.Assert("range-content-length", length == int(resp.ContentLength))
		} else {
			common.Assert("range-size-200", check_size == int(resp.ContentLength))
			common.Assert("range-md5-200", range_md5 == md5Response(resp, true))
		}
		if cc != nil {
			cc(resp, err)
		}
	})
}
func checkRespRange(resp *http.Response, from int, to int) {
	//fmt.Printf("from=[%d],to=[%d]\n", from, to)
	buf, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("read resp error[%s]\n", err.Error())
		return
	}
	buf2 := ReadRange(from, to, false)
	common.AssertByteSame(buf, buf2)
}
func md5FileRange(file string, from, to int) string {
	buf := ReadFileRange(file, from, to)
	if buf != nil {
		//fmt.Printf("md5File=[%s]\n", string(buf))
		result := md5sum(buf)
		//fmt.Printf("result=[%s]\n", result)
		return result
	}
	return ""
}
func md5File(from, to int, gzip bool) string {
	buf := ReadRange(from, to, gzip)
	if buf != nil {
		//fmt.Printf("md5File=[%s]\n", string(buf))
		result := md5sum(buf)
		//fmt.Printf("result=[%s]\n", result)
		return result
	}
	return ""
}
func md5Response2(resp *http.Response) (length int, md5_result string) {
	buf := make([]byte, 1024)
	hash := md5.New()
	var total_read int
	for {
		n, err := resp.Body.Read(buf)
		if n <= 0 {
			break
		}
		total_read += n
		//fmt.Printf("md5Response=[%s]\n", string(buf[0:n]))
		hash.Write(buf[0:n])
		if err != nil {
			break
		}
	}
	//fmt.Printf("total_read=[%d]\n", total_read)
	//AssertSame(total_read, int(resp.ContentLength))
	result := fmt.Sprintf("%x", hash.Sum(nil))
	//fmt.Printf("md5Respons=[%s]\n", result)
	return total_read, result
}
func md5Response(resp *http.Response, decode bool) string {
	buf := make([]byte, 1024)
	hash := md5.New()
	var total_read int
	content_encoding := resp.Header.Get("Content-Encoding")
	//fmt.Printf("content_encoding: [%v]\n", content_encoding)
	var reader io.Reader
	reader = resp.Body
	if decode && content_encoding == "gzip" {
		reader, _ = gzip.NewReader(resp.Body)
	}
	//fmt.Printf("\n")
	for {
		n, err := reader.Read(buf)
		if n <= 0 {
			break
		}
		total_read += n
		//fmt.Printf("%s", string(buf[0:n]))
		hash.Write(buf[0:n])
		if err != nil {
			break
		}
	}
	//fmt.Printf("\ntotal_read=[%d]\n", total_read)
	//common.AssertSame(total_read, int(resp.ContentLength))
	result := fmt.Sprintf("%x", hash.Sum(nil))
	//fmt.Printf("md5Respons=[%s]\n", result)
	return result
}
func md5sum(buf []byte) string {
	hash := md5.New()
	hash.Write(buf)
	return fmt.Sprintf("%x", hash.Sum(nil))
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
