package base_suite

import (
	"bufio"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"strings"
	"test_server/common"
	"time"
)

func handle_100_continue(w http.ResponseWriter, r *http.Request) {
	if r.Header.Get("x-read-body") == "1" {
		buf, _ := ioutil.ReadAll(r.Body)
		w.Write(buf)
		return
	}
	w.Write([]byte("ok\r\n"))
}
func check_100_continue_port(port int) {
	var str, body string
	var h map[string]string

	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", port))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	//not read body
	str = "POST /upstream/http/100_continue HTTP/1.1\r\nContent-Length: 4\r\nHost: localhost\r\nExpect: 100-continue\r\nx-read-body: 0\r\n\r\n"

	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, body, err = common.ReadHttpProtocol(reader, true)
	common.AssertSame(strings.ToLower(h["http/1.1"]), "200 ok")
	common.AssertSame(body, "ok")
	common.AssertSame(err, nil)

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
