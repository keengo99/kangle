package base_suite

import (
	"test_server/common"
	"test_server/kangle"

	//	"fmt"
	"net/http"
	"strings"
)

func check_big_vary() {
	//fmt.Printf("check big vary...\n")
	common.Get("/range?vary=Origin", map[string]string{"Origin": "test1"}, func(resp *http.Response, err error) {
		common.Assert("big-vary-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
	})
}
func check_change_vary() {
	kangle.CleanAllCache()
	common.Get("/vary", map[string]string{}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "no-vary")
	})
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test1", "Cache-Control": "no-cache"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "test1")
	})
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test1"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(common.Read(resp), "test1")
	})
}
func check_vary() {

	//fmt.Printf("check vary...\n")
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test1"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "test1")
	})
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test1"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(common.Read(resp), "test1")
	})
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test2"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "test2")
	})
	common.Get("/vary", map[string]string{"x-set-vary": "origin", "Origin": "test2"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(common.Read(resp), "test2")
	})
	//check vary change
	common.Get("/vary", map[string]string{"x-set-vary": "origin2", "Origin2": "test"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "test")
	})
	//change vary to origin2
	common.Get("/vary", map[string]string{"x-set-vary": "origin2", "Origin": "test2"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "")
	})
	common.Get("/vary", map[string]string{}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(common.Read(resp), "")
	})

	//change vary to null
	common.Get("/vary", map[string]string{"origin2": "test3"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change-miss", strings.Contains(resp.Header.Get("X-Cache"), "MISS "))
		common.AssertSame(common.Read(resp), "no-vary")
	})
	common.Get("/vary", map[string]string{"Origin": "test2"}, func(resp *http.Response, err error) {
		common.Assert("vary-x-cache-change-hit", strings.Contains(resp.Header.Get("X-Cache"), "HIT "))
		common.AssertSame(common.Read(resp), "no-vary")
	})
	check_change_vary()
}
