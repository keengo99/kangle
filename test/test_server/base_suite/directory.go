package base_suite

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"strings"
	"test_server/common"
	"time"
)

func check_directory_index() {
	var index_file_length int
	common.Get("/static/", nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		index_file_length = int(resp.ContentLength)
	})
	common.Get("/static/index.html", nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(index_file_length, int(resp.ContentLength))
	})
}
func check_directory_redirect() {
	var str string
	var h map[string]string

	port := 9999
	cn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%v", port))
	common.Assert("err", err == nil)
	defer cn.Close()
	reader := bufio.NewReader(cn)

	//not read body
	str = "GET /static HTTP/1.1\r\nHost: localhost\r\n\r\n"
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	h, _, err = common.ReadHttpProtocol(reader, false)
	common.AssertContain(strings.ToLower(h["http/1.1"]), "302")
	common.AssertSame(h["location"], "/static/")
	common.AssertSame(err, nil)

}
func check_directory() {
	check_directory_redirect()
	check_directory_index()
}
