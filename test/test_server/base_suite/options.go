package base_suite

import (
	"bufio"
	"fmt"
	"net"
	"strings"
	"test_server/common"
	"time"
)

func test_options_all() {
	var str, _ string
	var h map[string]string

	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", 9999))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	str = "OPTIONS * HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"

	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertSame(err, nil)
	common.AssertSame(strings.ToLower(h["http/1.1"]), "200 ok")
	common.AssertContain(strings.ToLower(h["allow"]), "get")
}

func test_options_method() {
	test_options_all()
}
