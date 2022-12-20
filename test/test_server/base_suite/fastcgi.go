package base_suite

import (
	"fmt"
	"net/http"
	"time"
	"strings"
	"test_server/common"
)

var x_time int

func check_fastcgi() {

	common.Get("/fastcgi/304.test", map[string]string{"x-time": fmt.Sprintf("%v", x_time)}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
	x_time++
	time.Sleep(time.Millisecond * 50)
	common.Get("/fastcgi/304.test", map[string]string{"pragma": "no-cache", "x-time": fmt.Sprintf("%v", x_time)}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	x_time++
	time.Sleep(time.Millisecond * 50)
	common.Get("/fastcgi/304.test", map[string]string{"pragma": "no-cache", "x-time": fmt.Sprintf("%v", x_time)}, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "hello")
		common.Assert("x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
	})
	x_time++
}
