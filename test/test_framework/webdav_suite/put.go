package webdav_suite

import (
	"bufio"
	"encoding/base64"
	"fmt"
	"net"
	"os"
	"strings"
	"test_framework/common"
	"test_framework/config"
	"time"
)

func dav_put_raw(name string, header string, body_parts []string, wait_continue bool) string {
	dir := config.Cfg.BasePath + "/www/dav/"
	os.MkdirAll(dir, 0755)
	os.Remove(dir + name)
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("dial", err == nil)
	defer cn.Close()
	auth := base64.StdEncoding.EncodeToString([]byte("test:test"))
	req := fmt.Sprintf("PUT /dav/%s HTTP/1.1\r\nHost: %s\r\nAuthorization: Basic %s\r\nConnection: close\r\n%s\r\n", name, config.GetLocalhost("webdav"), auth, header)
	cn.SetDeadline(time.Now().Add(5 * time.Second))
	cn.Write([]byte(req))
	reader := bufio.NewReader(cn)
	if wait_continue {
		line, _ := reader.ReadString('\n')
		common.Assert("100-continue", strings.Contains(line, " 100 "))
		for {
			line, err = reader.ReadString('\n')
			if err != nil || line == "\r\n" {
				break
			}
		}
	}
	for _, part := range body_parts {
		cn.Write([]byte(part))
	}
	line, _ := reader.ReadString('\n')
	return line
}
func check_dav_file(name string, content string) {
	file := config.Cfg.BasePath + "/www/dav/" + name
	data, err := os.ReadFile(file)
	common.Assert("read "+name, err == nil)
	common.AssertSame(string(data), content)
	os.Remove(file)
}
func check_put_chunked() {
	name := "chunked_put.txt"
	status := dav_put_raw(name, "Transfer-Encoding: chunked\r\n", []string{"5\r\nhello\r\n", "6\r\n world\r\n", "0\r\n\r\n"}, false)
	common.Assert("chunked put status", strings.Contains(status, " 201 "))
	check_dav_file(name, "hello world")
}
func check_put_expect() {
	name := "expect_put.txt"
	status := dav_put_raw(name, "Content-Length: 6\r\nExpect: 100-continue\r\n", []string{"expect"}, true)
	common.Assert("expect put status", strings.Contains(status, " 201 "))
	check_dav_file(name, "expect")
}
