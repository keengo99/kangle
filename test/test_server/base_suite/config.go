package base_suite

import (
	"test_server/common"
	"test_server/kangle"
)

func check_config() {
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
