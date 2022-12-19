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
	common.Request("OPTIONS", "http://webdav.test:9999/dav", "127.0.0.1", nil, func(resp *http.Response, err error) {
		common.AssertContain(resp.Header.Get("DAV"), "1")
	})
}
func check_client() {
	webdav := exec.Command(config.Cfg.BasePath+"/bin/webdav_client_example"+common.ExeExtendFile(), "http://webdav.test:9999/dav", "127.0.0.1")
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
