package common

import (
	"fmt"
	"net"
	"runtime"
	"strings"
	"test_server/config"
	"time"
)

var success_count, failed_count int

func AssertByteSame(a []byte, b []byte) {
	AssertSame(len(a), len(b))
	for i := 0; i < len(a); i++ {
		if !my_assert(a[i] == b[i]) {
			fmt.Printf("byte diffrent start=[%v]\n", i)
			return
		}
	}
}
func AssertContain(a string, sub string) {
	Assert(fmt.Sprintf("[%s] must contain [%s]", a, sub), strings.Contains(a, sub))
}
func AssertSame(a interface{}, b interface{}) {
	Assert(fmt.Sprintf("[%v] must same [%v]", a, b), a == b)
}
func my_assert(expression bool) bool {
	return Assert("", expression)
}
func AssertPort(port int, opened bool) {
	cn, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%v", port), 100*time.Millisecond)
	if cn != nil {
		defer cn.Close()
	}
	if opened {
		Assert("port must open", err == nil)
	} else {
		Assert("port must close", err != nil)
	}
}
func Assert(test_name string, expression bool) bool {
	if !expression {
		failed_count++
		if !config.Cfg.Force {
			fmt.Printf("\n")
			panic(test_name)
		}
		buf := make([]byte, 4096)
		runtime.Stack(buf, false)
		fmt.Printf("%s--\n%s\n", test_name, buf)
		return false
	}
	success_count++
	return true
}
func Report() {
	fmt.Printf("success_count=[%d],failed_count=[%d]\n", success_count, failed_count)
	if failed_count == 0 {
		fmt.Printf("***************test passed**********\n")
	} else {
		fmt.Printf("@@@@@@@@@@@@@@@test failed@@@@@@@@@@\n")
	}
}
