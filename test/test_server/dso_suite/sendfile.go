package dso_suite

import (
	"bufio"
	"net/http"
	"strconv"
	"test_server/common"
	"test_server/config"
)

func get_sendfile_size() (int64, int64) {
	url := config.GetUrl("localhost", "/dso/sendfile_report")
	client := common.GetClient(url)
	resp, err := client.Get(url)
	common.AssertSame(err, nil)
	defer resp.Body.Close()
	reader := bufio.NewReader(resp.Body)
	line1, _, _ := reader.ReadLine()
	line2, _, _ := reader.ReadLine()
	sendfile_count, _ := strconv.ParseInt(string(line1), 10, 64)
	sendfile_length, _ := strconv.ParseInt(string(line2), 10, 64)
	return sendfile_count, sendfile_length
}

/*
	func check_bigobj_sendfile() {
		common.Get("/range?sendfile", map[string]string{"pragma": "no-cache", "x-sendfile-test": "1"}, func(resp *http.Response, err error) {
			common.AssertContain(resp.Header.Get("X-Cache"), "MISS ")
			common.AssertSame(common.RangeMd5, common.Md5Response(resp, true))
		})
		common.Get("/range?sendfile", map[string]string{"x-sendfile-test": "1"}, func(resp *http.Response, err error) {
			common.AssertSame(common.RangeMd5, common.Md5Response(resp, true))
			common.AssertContain(resp.Header.Get("X-Cache"), "HIT ")
		})
	}
*/
func check_sendfile_with_alpn(alpn int) {
	config.Push()
	defer config.Pop()
	config.UseHttpClient(alpn)

	_, length := get_sendfile_size()
	var resp_length int64
	common.Get("/static/index.id?no-cache", map[string]string{"x-sendfile-test": "1", "x-cache": "no-cache"}, func(resp *http.Response, err error) {
		resp_length = resp.ContentLength
		common.Assert("length>0", resp_length > 0)
		common.AssertSame(resp.StatusCode, 200)
	})

	_, length2 := get_sendfile_size()
	common.AssertSame(length+resp_length, length2)
	/*
		//filter暂时还无法track cache发送，无法测试.
		check_bigobj_sendfile()
		_, length3 := get_sendfile_size()
		common.AssertSame(length2+int64(common.RangeSize), length3)
	*/
}
func check_sendfile() {
	check_sendfile_with_alpn(config.HTTP1)
	check_sendfile_with_alpn(config.H2C)
}
