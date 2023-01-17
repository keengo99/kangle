package base_suite

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"strings"
	"test_server/common"
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
	common.Assert("cann't read body", err != nil)
	common.AssertSame(len(line), 0)
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
}
