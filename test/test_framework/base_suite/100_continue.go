package base_suite

import (
	"bufio"
	"fmt"
	"io"
	"net"
	"net/http"
	"strings"
	"test_framework/common"
	"time"
)

func handle_100_continue(w http.ResponseWriter, r *http.Request) {
	if r.Header.Get("x-read-body") == "1" {
		buf, _ := io.ReadAll(r.Body)
		w.Write(buf)
		return
	}
	w.Write([]byte("ok\r\n"))
}
func check_100_continue_port(port int) {
	var str, body string
	var h map[string]string
	addr := fmt.Sprintf("127.0.0.1:%v", port)

	cn, err := net.Dial("tcp", addr)
	common.Assert("err", err == nil)
	reader := bufio.NewReader(cn)

	//not read body
	str = "POST /upstream/http/100_continue HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\nExpect: 100-continue\r\nx-read-body: 0\r\n\r\n"

	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, body, err = common.ReadHttpProtocol(reader, true)
	common.AssertSame(strings.ToLower(h["http/1.1"]), "200 ok")
	common.AssertSame(body, "ok")
	common.AssertSame(err, nil)
	cn.Close()

	// The previous request intentionally did not send its declared body.  A
	// compliant server closes that connection, so use a fresh one for the
	// request that exercises the 100 Continue handshake.
	cn, err = net.Dial("tcp", addr)
	common.Assert("err", err == nil)
	defer cn.Close()
	reader = bufio.NewReader(cn)

	//read body
	str = "POST /upstream/http/100_continue HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\nExpect: 100-continue\r\nx-read-body: 1\r\n\r\n"
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	//first must be 100 continue
	common.AssertSame(strings.ToLower(h["http/1.1"]), "100 continue")
	common.AssertSame(err, nil)
	cn.Write([]byte("ko\r\n"))
	//then status is 200 ok
	h, body, _ = common.ReadHttpProtocol(reader, true)
	common.AssertSame(strings.ToLower(h["http/1.1"]), "200 ok")
	common.AssertSame(body, "ko")

}

func check_100_continue() {
	check_100_continue_port(9999)
	check_100_continue_port(9900)
}

func check_fastcgi_post() {
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("dial FastCGI Expect request", err == nil)
	reader := bufio.NewReader(cn)
	cn.SetDeadline(time.Now().Add(5 * time.Second))
	_, _ = cn.Write([]byte("POST /fastcgi/expect HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\nExpect: 100-continue\r\nConnection: close\r\n\r\n"))
	h, _, err := common.ReadHttpProtocol(reader, false)
	common.Assert("FastCGI 100-continue response", err == nil && strings.EqualFold(h["http/1.1"], "100 continue"))
	_, _ = cn.Write([]byte("body"))
	h, _, err = common.ReadHttpProtocol(reader, true)
	common.Assert("FastCGI Expect final response", err == nil && strings.EqualFold(h["http/1.1"], "200 ok"))
	cn.Close()

	cn, err = net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("dial FastCGI chunked request", err == nil)
	defer cn.Close()
	reader = bufio.NewReader(cn)
	cn.SetDeadline(time.Now().Add(5 * time.Second))
	_, _ = cn.Write([]byte("POST /fastcgi/chunked HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n4\r\nbody\r\n0\r\n\r\n"))
	h, _, err = common.ReadHttpProtocol(reader, true)
	common.Assert("FastCGI chunked response", err == nil && strings.EqualFold(h["http/1.1"], "200 ok"))
}
