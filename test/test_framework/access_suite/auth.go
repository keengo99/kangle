package access_suite

import (
	"crypto/md5"
	"fmt"
	"net/http"
	"regexp"
	"test_framework/common"
	"test_framework/config"
)

func handle_auth(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("ok"))
}
func digestValue(value string) string {
	return fmt.Sprintf("%x", md5.Sum([]byte(value)))
}
func digestField(challenge string, name string) string {
	match := regexp.MustCompile(name + `="([^"]+)"`).FindStringSubmatch(challenge)
	if len(match) != 2 {
		panic("missing digest field " + name)
	}
	return match[1]
}
func check_http_auth() {
	common.Getx("/auth_load_failed", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 500)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), map[string]string{"AUTH_BASIC": "user,pass2"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), map[string]string{"AUTH_BASIC": "user,pass"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	common.Getx("/access/auth", config.GetLocalhost("access"), map[string]string{"Authorization": "B dXNlcjpwYXNz"}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})

	challenge := ""
	common.Getx("/access/digest", config.GetLocalhost("access"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
		challenge = resp.Header.Get("WWW-Authenticate")
	})
	realm := digestField(challenge, "realm")
	nonce := digestField(challenge, "nonce")
	uri := "/access/digest"
	nc := "00000001"
	cnonce := "kangle-test"
	qop := "auth"
	ha1 := digestValue("user:" + realm + ":pass")
	ha2 := digestValue("GET:" + uri)
	response := digestValue(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2)
	authorization := fmt.Sprintf(`Digest username="user", realm="%s", nonce="%s", uri="%s", response="%s", qop=%s, nc=%s, cnonce="%s"`, realm, nonce, uri, response, qop, nc, cnonce)
	common.Getx(uri, config.GetLocalhost("access"), map[string]string{"Authorization": authorization}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	common.Getx(uri+"?different=1", config.GetLocalhost("access"), map[string]string{"Authorization": authorization}, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 401)
	})
}
