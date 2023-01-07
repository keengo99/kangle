package base_suite

import (
	"io/ioutil"
	"net"
	"net/http"
	"strings"
	"test_server/common"
	"time"
)

func handle_obs_fold(w http.ResponseWriter, r *http.Request) {
	//fmt.Printf("x-obs=%v", r.Header.Get("x-obs"))
	w.Write([]byte("x-obs="))
	w.Write([]byte(r.Header.Get("x-obs")))
}
func check_obs_fold() {
	str := "GET /upstream/http/obs_fold HTTP/1.0\r\nContent-Length: 0\r\nHost: localhost\r\nx-obs: hello,\r\n world\r\nUser-agent: hello\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("obs_fold", strings.Contains(str, "\nx-obs=hello,   world"))
	cn.Close()

	//check split obs_fold
	str1 := "GET /upstream/http/obs_fold HTTP/1.0\r\nContent-Length: 0\r\nHost: localhost\r\nx-obs: hello,\r\n"
	str2 := " world\r\nUser-agent: hello\r\n\r\n"
	cn, err = net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)

	cn.Write([]byte(str1))
	time.Sleep(time.Millisecond * 10)
	cn.Write([]byte(str2))

	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ = ioutil.ReadAll(cn)
	str = string(buf)
	common.Assert("obs_fold", strings.Contains(str, "\nx-obs=hello,   world"))
	cn.Close()
}
