package base_suite

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"strings"
	"test_framework/common"
	"time"
)

func HandleFlush(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("Content-Length", "10")
	w.Header().Add("X-No-Buffer", "ON")
	f, _ := w.(http.Flusher)
	f.Flush()
	for i := 0; i < 10; i++ {
		fmt.Fprintf(w, "%v", i)
		f.Flush()
		time.Sleep(time.Millisecond * 100)
	}
}

func check_flush() {
	var str string
	var h map[string]string

	port := 9999
	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", port))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	//not read body
	str = "GET /upstream/http/flush HTTP/1.1\r\nHost: localhost\r\n\r\n"
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertContain(strings.ToLower(h["http/1.1"]), "200")
	common.AssertSame(err, nil)
	for i := 0; i < 10; i++ {
		buf := make([]byte, 128)
		n, _ := reader.Read(buf)
		common.AssertSame(n, 1)
		v := string(buf[0])
		common.AssertSame(v, fmt.Sprintf("%v", i))
	}
}
