package vh_suite

import (
	"net/http"
	"test_server/common"
	"test_server/config"
	"test_server/kangle"
)

func handle_alias_x_sendfile(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("X-Accel-Redirect", "/internal_alias/index.html")
}
func handle_map(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("map_is_ok"))
}
func check_map() {
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<vhs>
			<map path='/map_path' extend='server:map_path' confirm_file='0' allow_method='*'/>
			<map file_ext='map_file' extend='server:map_path' confirm_file='0' allow_method='*'/>
		</vhs>
		<server name='map_path' host='127.0.0.1' port='4411' proto='http' life_time='10'/>
	</config>	
	`)
	kangle.Reload("ext|10.xml_")
	common.Getx("/map_path/test", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(common.Read(resp), "map_is_ok")
	})
	common.Getx("/map_file/test.map_file", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertSame(common.Read(resp), "map_is_ok")
	})
}
func check_index() {
	common.Getx("/static/", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<vhs>
		</vhs>
	</config>	
	`)
	kangle.Reload("ext|10.xml_")
	common.Getx("/static/", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
}
func check_alias() {
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<server name='map_path' host='127.0.0.1' port='4411' proto='http' life_time='10'/>
		<vhs>
			<map path='/map_path' extend='server:map_path' confirm_file='0' allow_method='*'/>
			<alias path='/alias' to='static'/>
			<alias path='/internal_alias' to='static' internal='1'/>
		</vhs>
	</config>	
	`)
	kangle.Reload("ext|10.xml_")
	common.Getx("/alias/index.html", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
	})
	common.Getx("/internal_alias/index.html", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 404)
	})
	common.Getx("/map_path/alias-x-sendfile", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 200)
		common.AssertContain(common.Read(resp), "welcome use kangle!")
	})
}
func check_error_code() {
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<vhs>
			<error code='404' file='/404.html'/>
			<error code='403' file='/403.html'/>
		</vhs>
	</config>	
	`)
	kangle.Reload("ext|10.xml_")
	common.Getx("/static/no_exsit", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 404)
		common.AssertSame(common.Read(resp), "404")
	})
	common.Getx("/dav/", config.GetLocalhost("vh"), nil, func(resp *http.Response, err error) {
		common.AssertSame(resp.StatusCode, 403)
		common.AssertSame(common.Read(resp), "403")
	})
}
