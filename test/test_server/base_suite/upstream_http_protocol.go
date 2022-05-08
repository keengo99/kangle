package base_suite

import (
	"fmt"
	"net/http"
	"test_server/common"
	"test_server/config"
	"time"
)

func HandleUpstreamHttpProtocol(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString("HTTP/1.1 200 OK\r\nTransfer")
	wb.Flush()
	time.Sleep(time.Millisecond * 20)
	wb.WriteString("-Encoding: chunked\r\nConnec")
	time.Sleep(time.Millisecond * 20)
	wb.Flush()
	time.Sleep(time.Millisecond * 20)
	wb.WriteString("tion: close\r\n\r\n2\r\no")
	time.Sleep(time.Millisecond * 20)
	wb.Flush()
	time.Sleep(time.Millisecond * 20)
	wb.WriteString("k\r\n0\r\n\r\n")
	wb.Flush()
	cn.Close()
}
func check_upstream_http_protocol() {

	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	common.Get("/upstream_http_protocol", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "ok")
	})
}
