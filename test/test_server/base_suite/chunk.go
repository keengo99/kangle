package base_suite

import (
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"test_server/common"
	"test_server/config"

	"strings"
	"time"
)

func check_chunk_post() {
	str := fmt.Sprintf("PUT /chunk_post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nte\r\n2\r\nst\r\n0\r\n\r\n")
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	fmt.Printf("resp=[%v]\n", str)
	common.Assert("chunk_post", strings.Index(str, "\ntest") > 0)
}
func check_http2_post_form() {
	common.Post("/chunk_post", map[string]string{"Transfer-Encoding": "chunked", "Content-Type": "application/x-www-form-urlencoded"}, "a=OK", func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "OK")
	})
}
func check_chunk_post_form() {
	//fmt.Printf("check_chunk_post_form...\n")
	str := "POST /a.php HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\nHost: localhost\r\n\r\n10\r\na=OK0123456789ab\r\n2\r\ncd\r\n0\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	//*
	l := len(str)
	for i := 100; i < l-40; i++ {
		check_split_post(str, i, func(buf string) {
			common.Assert(fmt.Sprintf("chunk_post_form_%d", i), strings.Index(buf, "\nOK0123456789abcd") > 0 && strings.Index(buf, "\nOK") != strings.LastIndex(buf, "\nOK"))
		})

	}
	////*/
	//check_split_post(str, 119)
	/*
		post("/a.php", map[string]string{"Transfer-Encoding": "chunked", "Content-Type": "application/x-www-form-urlencoded"}, "a=b", func(resp *http.Response, err error) {
			AssertSame(read(resp), "b")
		})
		/*
			str := fmt.Sprintf("POST /a.php HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n\r\n3\r\na=b\r\n0\r\n\r\n")
			cn, err := net.Dial("tcp", "127.0.0.1:9999")
			Assert("err", err == nil)
			defer cn.Close()
			cn.Write([]byte(str))
			cn.SetReadDeadline(time.Now().Add(2 * time.Second))
			buf, _ := ioutil.ReadAll(cn)
			str = string(buf)
			fmt.Printf("resp=[%v]\n", str)
			Assert("chunk_post", strings.Index(str, "\nb") > 0)
	*/
}
func HandleChunkUpstream(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n2\r\nok\r\n0\r\n\r\n")
	wb.Flush()
	cn.Close()
}
func check_chunk_upstream() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	common.Get("/chunk", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "ok")
	})
}
