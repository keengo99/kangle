package access_suite

import (
	"bufio"
	"fmt"
	"net"
	"strings"
	"test_server/common"
	"time"
)

func check_redirect() {
	var str string
	var h map[string]string

	port := 9999
	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", port))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	//not read body
	str = "GET /redirect HTTP/1.1\r\nHost: access.localtest.me\r\n\r\n"
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertContain(strings.ToLower(h["http/1.1"]), "302")
	common.AssertSame(h["location"], "http://redirect.localtest.me:9999/")
	common.AssertSame(err, nil)
}
func check_rewrite() {
	var str string
	var h map[string]string

	port := 9999
	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", port))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	//not read body
	str = "GET /rwaaa HTTP/1.1\r\nHost: access.localtest.me\r\n\r\n"
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertContain(strings.ToLower(h["http/1.1"]), "302")
	common.AssertSame(h["location"], "/wraaa")
	common.AssertSame(err, nil)
}
