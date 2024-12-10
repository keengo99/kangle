package base_suite

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"strconv"
	"strings"
	"test_framework/common"
	"time"
)

func test_head_404() {
	var str, _ string
	var h map[string]string

	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", 9999))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	str = "HEAD /static/404.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertSame(err, nil)
	line, _, err := reader.ReadLine()
	common.AssertSame(strings.ToLower(h["http/1.1"]), "404 not found")
	common.Assert("connection must be close", err != nil)
	common.AssertSame(len(line), 0)
}
func test_head_keep_alive() {
	var str, _ string
	var h map[string]string

	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", 9999))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	str = "HEAD /static/index.html HTTP/1.1\r\nHost: localhost\r\n\r\nGET /static/index.html HTTP/1.0\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"

	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertSame(err, nil)
	s, ok := h["content-length"]
	common.Assert("must have content-length", ok)
	content_length, _ := strconv.ParseInt(s, 10, 32)
	common.Assert("content_length must greater 0", content_length > 0)
	_, ok = h["connection"]
	common.Assert("http/1.1 keep-alive don't need header connection", !ok)
	line, _, err := reader.ReadLine()
	common.Assert("read body must be success", err == nil)
	common.AssertContain(strings.ToLower(string(line)), "http/1.")
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertSame(err, nil)
	common.AssertContain(h["connection"], "keep-alive")
}
func test_head_method() {

	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, nil)
	common.Head("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, nil)

	common.Get("/static/index.html", map[string]string{"Accept-Encoding": "identity"}, func(resp *http.Response, err error) {
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})

	common.Get("/static/index.html", map[string]string{"Range": "bytes=102400-", "Accept-Encoding": "identity"},
		func(resp *http.Response, err error) {
			common.AssertSame(resp.StatusCode, 416)
		})
	test_head_404()
	test_head_keep_alive()
}
