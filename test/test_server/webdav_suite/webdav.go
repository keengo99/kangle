package webdav_suite

import (
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"test_server/common"
	"test_server/config"
)

func check_options() {
	common.Request("OPTIONS", "/dav", config.GetLocalhost("webdav"), nil, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("DAV"), "1")
	})
}
func check_client() {
	webdav := exec.Command(config.Cfg.BasePath+"/bin/webdav_client_example"+common.ExeExtendFile(), fmt.Sprintf("http://%s:9999/dav", config.GetLocalhost("webdav")))
	webdav.Stdout = os.Stdout
	webdav.Stderr = os.Stderr
	err := webdav.Start()
	if err != nil {
		panic(err)
	}
	err = webdav.Wait()
	fmt.Printf("webdav test end\n")
	if err != nil {
		panic(err)
	}
}
