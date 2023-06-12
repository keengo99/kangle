package base_suite

import (
	"fmt"
	"net"
	"net/http"
	"strings"
	"test_framework/common"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

func HandleWebsocket(res http.ResponseWriter, req *http.Request) {
	if req.FormValue("port") == "9900" {
		//检查via,确保经过了http2代理过来的。
		via := req.Header.Get("Via")
		//fmt.Printf("via=[%v]\n", via)
		vias := strings.Split(via, ", ")
		common.AssertSame(len(vias), 2)
		item := strings.Split(vias[1], " ")
		common.AssertSame(item[0], "2")
	}
	ws, err := upgrader.Upgrade(res, req, nil)
	if err != nil {
		panic(err.Error())
	}
	for {
		messageType, messageContent, err := ws.ReadMessage()
		if err != nil {
			//fmt.Printf("read message error [%v]\n", err.Error())
			return
		}
		if err := ws.WriteMessage(messageType, []byte(messageContent)); err != nil {
			panic(err.Error())
		}
	}
}
func HandleWebsocketHijack(w http.ResponseWriter, r *http.Request) {
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
		common.AssertSame(string(buf[0:n]), "ok")
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
	n, _ := cn.Read(buf)
	common.AssertSame(string(buf[0:n]), "hello")
}
func check_server_first_websocket(host string) {
	cn, err := net.Dial("tcp", host)
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte("GET /websocket?first=server HTTP/1.1\r\nHost: localhost\r\nConnection: upgrade\r\n\r\n"))
	buf := make([]byte, 512)
	cn.Read(buf)
	n, _ := cn.Read(buf)
	cn.Write(buf[0:n])
}
func test_websocket() {
	check_client_first_websocket("127.0.0.1:9999")
	check_server_first_websocket("127.0.0.1:9999")
}
func test_upstream_disable_h2_websocket() {
	check_client_first_websocket("127.0.0.1:9801")
	check_server_first_websocket("127.0.0.1:9801")
}
func check_websocket_over_h2(port int) {
	if port == 9900 {
		//确保第一次连接后能判断出http2是否支持websocket.
		common.Get("http://localhost:9900/upstream/http/etag?websocket", nil, nil)
	}
	conn, _, err := websocket.DefaultDialer.Dial(fmt.Sprintf("ws://localhost:%v/upstream/http/websocket2?port=%v", port, port), nil)
	common.AssertSame(err, nil)
	defer conn.Close()
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	conn.WriteMessage(1, []byte("hello"))
	msgType, msg, err := conn.ReadMessage()
	if err != nil {
		panic((err.Error()))
	}
	common.AssertSame(msgType, 1)
	common.AssertSame(string(msg), "hello")
}
func test_websocket_over_h2() {
	check_websocket_over_h2(9999) //一次代理
	check_websocket_over_h2(9900) //两次代理
}
