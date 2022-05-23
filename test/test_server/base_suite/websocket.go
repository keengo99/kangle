package base_suite

import (
	"net"
	"net/http"
	"test_server/common"
	"time"
)

func HandleWebsocket(w http.ResponseWriter, r *http.Request) {
	r.ParseForm()
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString("HTTP/1.1 200\r\nConnection: upgrade\r\n\r\n")
	wb.Flush()
	time.Sleep(200 * time.Millisecond)
	if r.Form.Get("first") == "client" {
		buf := make([]byte, 8)
		n, err := wb.Read(buf)
		common.AssertSame(err, nil)
		if n > 0 {
			wb.Write(buf[0:n])
		}
		wb.Flush()
	} else {
		time.Sleep(500 * time.Millisecond)
		wb.WriteString("ok")
		wb.Flush()
		buf := make([]byte, 8)
		n, err := wb.Read(buf)
		common.AssertSame(err, nil)
		common.Assert("websocket", string(buf[0:n]) == "ok")
	}
	cn.Close()
}
func check_client_first_websocket(host string) {
	cn, err := net.Dial("tcp", host)
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte("GET /websocket?first=client HTTP/1.1\r\nHost: localhost\r\nConnection: upgrade\r\n\r\n"))
	buf := make([]byte, 512)
	cn.Read(buf)
	cn.Write([]byte("hello"))
	n, err := cn.Read(buf)
	common.AssertSame(string(buf[0:n]), "hello")
}
func check_server_first_websocket(host string) {
	cn, err := net.Dial("tcp", host)
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte("GET /websocket?first=server HTTP/1.1\r\nHost: localhost\r\nConnection: upgrade\r\n\r\n"))
	buf := make([]byte, 512)
	cn.Read(buf)
	n, err := cn.Read(buf)
	cn.Write(buf[0:n])
}
func test_websocket() {
	check_client_first_websocket("127.0.0.1:9999")
	check_server_first_websocket("127.0.0.1:9999")
}
func Check() {
	//check_client_first_websocket()
	//check_server_first_websocket()
	println("check done...")
}
func test_upstream_h2_websocket() {
	check_client_first_websocket("127.0.0.1:9801")
	check_server_first_websocket("127.0.0.1:9801")
}
