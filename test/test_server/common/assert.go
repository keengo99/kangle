package common

import (
	"fmt"
	"runtime"
	"strings"
	"test_server/config"
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
	if !my_assert(strings.Contains(a, sub)) {
		fmt.Printf("[%s] not contain [%s]\n", a, sub)
	}
}
func AssertSame(a interface{}, b interface{}) {
	if a == b {
		return
	}
	fmt.Printf("[%v] not eq [%v]\n", a, b)
	Assert("", false)
}
func my_assert(expression bool) bool {
	return Assert("", expression)
}
func Assert(test_name string, expression bool) bool {
	if !expression {
		failed_count++
		if !config.Cfg.Force {
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
