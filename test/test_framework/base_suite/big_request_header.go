package base_suite

import (
	"fmt"
	"net"
	"test_framework/common"
	"time"
)

func check_big_request_header() {
	request := "GET /kangle.status HTTP/1.1\r\nUser-Agent: x-test\r\nReferer: http://localhost/\r\nHost: localhost\r\n"
	for i := 0; i < 10; i++ {
		request += fmt.Sprintf("Cookie%d: %s\r\n", string(common.RandStringBytes(50*1024)))
	}
	request += "\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9900")
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte(request))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	_, body, _ := common.ReadHttpProtocol2(cn, true)
	common.AssertSame(body, "OK")
}
