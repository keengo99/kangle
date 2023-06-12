package base_suite

import (
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"strconv"
	"strings"
	"test_framework/common"
	"test_framework/config"
	"time"
)

type check_string_callback func(string)

func check_split_post(str string, start int, fn check_string_callback) {
	cn, err := net.Dial("tcp", "127.0.0.1:9900")
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte(str[0:start]))
	time.Sleep(10 * time.Millisecond)
	cn.Write([]byte(str[start:]))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	fn(string(buf))
}
func check_http_1_1_split_chunk_post_pipe_line() {
	str := "POST /kangle.status HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\nHost: localhost\r\n\r\n10\r\na=OK0123456789ab\r\n2\r\ncd\r\n0\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	//*
	l := len(str)

	for i := 80; i < l-30; i++ {
		//fmt.Printf("pos=[%d]\n", i)
		check_split_post(str, i, func(buf string) {
			common.Assert(fmt.Sprintf("skip_chunk_post_%d", i), strings.Index(buf, "\nOK") > 0 && strings.Index(buf, "\nOK") != strings.LastIndex(buf, "\nOK"))
		})

	}
	//*/
	//check_split_post(str, 81)
}
func check_http_1_1_chunk_post_pipe_line() {
	str := "POST /kangle.status HTTP/1.1\r\nTransfer-Encoding: chunked\r\nHost: localhost\r\n\r\n4\r\nabcd\r\n0\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	//fmt.Println(str)
	common.Assert("chunk_post_pipe_line", strings.Index(str, "\nOK") > 0 && strings.Index(str, "\nOK") != strings.LastIndex(str, "\nOK"))
}
func check_http_1_1_skip_post_split_pipe_line() {
	str := "POST /kangle.status HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\n\r\nab"
	str2 := "cdGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	time.Sleep(100 * time.Millisecond)
	cn.Write([]byte(str2))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("skip_post_pipe_line", strings.Index(str, "\nOK") > 0 && strings.Index(str, "\nOK") != strings.LastIndex(str, "\nOK"))
}
func check_http_1_1_skip_post_pipe_line() {
	str := fmt.Sprintf("POST /kangle.status HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\n\r\nabcdGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("skip_post_pipe_line", strings.Index(str, "\nOK") > 0 && strings.Index(str, "\nOK") != strings.LastIndex(str, "\nOK"))
}
func check_http_1_1_post_pipe_line() {
	str := "POST /no-cache HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\n\r\nabcdGET /no-cache HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("post_pipe_line", strings.Index(str, "\nOK") > 0 && strings.Index(str, "\nOK") != strings.LastIndex(str, "\nOK"))
}
func check_http_1_1_get_pipe_line() {
	str := "GET /kangle.status HTTP/1.1\r\nHost: localhost\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("get_pipe_line", strings.Index(str, "\nOK") > 0 && strings.Index(str, "\nOK") != strings.LastIndex(str, "\nOK"))
}
func HandleSplitResponse(w http.ResponseWriter, r *http.Request) {
	s, _ := strconv.Atoi(r.FormValue("s"))
	//fmt.Printf("split response start=[%v]\n", s)
	l := len(SPLIT_RESPONSE)
	if s > l-1 {
		s = l - 1
	}
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString(SPLIT_RESPONSE[0:s])
	wb.Flush()
	time.Sleep(10 * time.Millisecond)
	wb.WriteString(SPLIT_RESPONSE[s:])
	wb.Flush()
	cn.Close()
}
func check_split_response2(start int) {
	path := fmt.Sprintf("/split_response?s=%d", start)
	common.Get(path, nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "OK")
	})
}
func check_split_response() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	config.Push()
	defer config.Pop()
	config.Cfg.UrlPrefix = "http://127.0.0.1:9999"
	check_split_response2(17)
	/*
		for i := 0; i < len(SPLIT_RESPONSE)-1; i++ {
			check_split_response2(i)
		}
	*/
}
func check_http_1_1_pipe_line() {
	//fmt.Printf("check_http_1_1_pipe_line...\n")
	//*
	check_http_1_1_get_pipe_line()
	check_http_1_1_post_pipe_line()
	check_http_1_1_skip_post_pipe_line()
	check_http_1_1_skip_post_split_pipe_line()
	check_http_1_1_chunk_post_pipe_line()
	//*/
	check_http_1_1_split_chunk_post_pipe_line()
}
