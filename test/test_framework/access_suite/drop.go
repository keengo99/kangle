package access_suite

import (
	"fmt"
	"net"
	"test_framework/common"
	"test_framework/config"
	"time"
)

func check_drop() {
	if config.Cfg.Alpn != config.HTTP1 {
		return
	}

	conn, err := net.DialTimeout("tcp", "127.0.0.1:9999", 3*time.Second)
	common.AssertSame(err, nil)
	if err != nil {
		return
	}
	defer conn.Close()

	_ = conn.SetDeadline(time.Now().Add(3 * time.Second))
	_, err = fmt.Fprintf(conn,
		"GET /drop-no-response HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n",
		config.GetLocalhost("access"))
	common.AssertSame(err, nil)
	if err != nil {
		return
	}

	buffer := make([]byte, 1)
	got, readErr := conn.Read(buffer)
	common.AssertSame(got, 0)
	if netErr, ok := readErr.(net.Error); ok && netErr.Timeout() {
		common.Assert("drop must close the connection instead of timing out", false)
		return
	}
	common.Assert("drop must close without an HTTP response", readErr != nil)
}
