package base_suite

import (
	"fmt"
	"net"
	"net/http"
	"test_framework/common"
	"test_framework/kangle"
)

func handle_self_addr(w http.ResponseWriter, r *http.Request) {
	ctx := r.Context()
	srvAddr := ctx.Value(http.LocalAddrContextKey).(net.Addr)
	w.Header().Add("Cache-Control", "no-cache, no-store")
	w.Write([]byte(srvAddr.String()))
	//fmt.Println(srvAddr)
	//hj, _ := w.(http.Hijacker)
	//cn, _, _ := hj.Hijack()
	//cn.Write([]byte("HTTP/1.1 200 OK\r\nConnection: close\r\nCache-Control: no-cache, no-store\r\n\r\n"))
	//cn.Write([]byte(cn.LocalAddr().String()))
	//cn.Close()
}
func create_config_file(server_cfg string) {
	kangle.CreateExtConfig("21", fmt.Sprintf(`<!--#start 21 -->
<config>
	<server name='upstream_changed'  proto='http'>
		<node weight=1 %s />
	</server>
	<request>
	<table name='BEGIN'>
		<chain action='server:upstream_changed'>
			<acl module='path' path='/self_addr'/>
		</chain>
	</table>
	</request>
</config>
<!--configfileisok-->
	`, server_cfg))
	kangle.Reload("ext|21.xml_")
}
func check_config_server() {
	create_config_file("host='127.0.0.1' port='4411' life_time='60'")
	common.Get("/self_addr", nil, func(resp *http.Response, err error) {
		common.AssertContain(common.Read(resp), ":4411")
	})
	create_config_file("host='127.0.0.1' port='4412s' life_time='60' ")
	common.Get("/self_addr", nil, func(resp *http.Response, err error) {
		common.AssertContain(common.Read(resp), ":4412")
	})
}
