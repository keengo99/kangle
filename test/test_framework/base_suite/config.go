package base_suite

import (
	"test_framework/common"
	"test_framework/config"
	"test_framework/kangle"
)

func check_listen_config() {
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<listen ip='127.0.0.1' port='7777' type='http'/>
	</config>	
	`)
	resp := kangle.Reload("ext|10.xml_")
	common.AssertSame(resp.StatusCode, 204)
	common.AssertPort(7777, true)
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>		
	</config>	
	`)
	resp = kangle.Reload("ext|10.xml_")
	common.AssertSame(resp.StatusCode, 204)
	common.AssertPort(7777, false)
}
func check_vh_config() {
	access_file := config.Cfg.BasePath + "/www/access.xml"
	kangle.CreateFile(access_file, `<config><request><table name='aaa'></table></request></config>`)
	kangle.CreateExtConfig("20", `<!--#start 20 -->
<config>
	<vh doc_root='www'  access='access.xml'>
		<host>vh_config</host>
		<request/>
	</vh>
</config>
<!--configfileisok-->
`)
	kangle.Reload("ext|20.xml_")

	kangle.CreateExtConfig("20", `<!--#start 20 -->
	<config>
		<vh doc_root='www'>
			<host>vh_config</host>
			<request/>
		</vh>
	</config>
	`)
	kangle.Reload("ext|20.xml_")

	kangle.CreateExtConfig("20", `<!--#start 20 -->
		<config>
			<vh doc_root='www'>
				<host>vh_config</host>
			</vh>
		</config>
		`)
	kangle.Reload("ext|20.xml_")

	kangle.ReloadConfig()
}
func check_config() {
	check_listen_config()
	check_vh_config()
}
