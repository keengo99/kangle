package suite

import (
	"fmt"
	"sort"
	"strings"
	"test_server/common"
)

func GetSuies() []string {
	names := make([]string, 0)
	for k, _ := range suites {
		names = append(names, k)
	}
	sort.Strings(names)
	return names
}
func List() {
	names := GetSuies()
	for _, name := range names {
		suite, _ := suites[name]
		suite.List()
	}
}
func SplitSuiteCase(name string) (suite string, testCase string) {
	strs := strings.Split(name, ".")
	if len(strs) == 2 {
		return strs[0], strs[1]
	}
	return name, ""
}
func Init(names []string) {
	for _, name := range names {
		s, _ := SplitSuiteCase(name)
		suite, ok := suites[s]
		if !ok {
			fmt.Printf("cann't findd suite [%s]\n", name)
			panic("")
		}
		common.AssertSame(ok, true)
		fmt.Printf("init suite [%s]\n", name)
		suite.Init()
	}
}
func Process(names []string) {
	for _, name := range names {
		s, c := SplitSuiteCase(name)
		suite, ok := suites[s]
		common.AssertSame(ok, true)
		suite.Test(c)
	}
}
func Clean(names []string) {
	for _, name := range names {
		s, _ := SplitSuiteCase(name)
		suite, ok := suites[s]
		common.AssertSame(ok, true)
		suite.Clean()
	}
}
