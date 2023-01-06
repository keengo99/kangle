package dso_suite

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"strings"
	"test_server/common"
	"test_server/config"
	"time"
)

func check_upstream() {
	common.Getx("/test_dso_upstream", config.GetLocalhost("dso"), nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "test_upstream_ok")
	})
}
func check_filter() {
	common.Getx("/test_dso_filter", config.GetLocalhost("dso"), nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "t*st_upstr*am_ok")
	})
}
func check_http10_chunk() {
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte(fmt.Sprintf("GET /dso/before_cache_chunk HTTP/1.0\r\nHost: %v\r\nConnection: keep-alive\r\n\r\n", config.GetLocalhost("dso"))))
	reader := bufio.NewReader(cn)
	cn.SetReadDeadline(time.Now().Add(time.Second))
	headers := make(map[string]string)
	for {
		line, _, err := reader.ReadLine()
		if err != nil {
			panic(err.Error())
		}
		if len(line) == 0 {
			break
		}
		kvs := strings.SplitN(string(line), " ", 2)
		headers[strings.ToLower(kvs[0])] = kvs[1]
	}
	line, _, _ := reader.ReadLine()
	v, ok := headers["connection:"]
	if ok {
		common.AssertSame(v, strings.ToLower("close"))
	}
	_, ok = headers["transfer-encoding:"]
	common.AssertSame(ok, false)
	common.AssertSame(string(line), "hello")
}

func check_before_cache_chunk() {
	common.Getx("/dso/before_cache_chunk", config.GetLocalhost("dso"), nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
	})
}
func test_dso_client_first(path string) {
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.AssertSame(err, nil)
	defer cn.Close()
	cn.Write([]byte(fmt.Sprintf("GET %v?first=client HTTP/1.1\r\nHost: %v\r\nConnection: upgrade\r\n\r\n", path, config.GetLocalhost("dso"))))
	cn.Write([]byte("hello\r\n"))
	reader := bufio.NewReader(cn)
	cn.SetReadDeadline(time.Now().Add(time.Second))
	for {
		line, _, err := reader.ReadLine()
		if err != nil {
			panic(err.Error())
		}
		if len(line) == 0 {
			break
		}
	}
	line, _, _ := reader.ReadLine()
	common.AssertSame(string(line), "hello")
}
func test_dso_websocket() {
	test_dso_client_first("/dso/bc_websocket")
	test_dso_client_first("/dso/websocket")
}
