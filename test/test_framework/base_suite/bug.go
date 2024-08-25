package base_suite

import (
	"fmt"
	"strings"
	"test_framework/common"
)

func check_bug() {
	str := "POST /kangle.status HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\nHost: localhost\r\n\r\n10\r\na=OK0123456789ab\r\n2\r\ncd\r\n0\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	check_split_post(str, 143, func(buf string) {
		//fmt.Printf("%v", buf)
		common.Assert(fmt.Sprintf("skip_chunk_post_143"), strings.Index(buf, "\nOK") > 0 && strings.Index(buf, "\nOK") != strings.LastIndex(buf, "\nOK"))
	})
}
