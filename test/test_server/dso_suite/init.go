package dso_suite

import (
	"fmt"
	"test_server/kangle"
	"test_server/suite"
)

type dso struct {
	suite.Suite
}

func (d *dso) Init() error {
	fmt.Printf("dso init\n")
	content := "<!--#start 300-->\r\n<config>\r\n"
	content += "<dso_extend name='testdso' filename='bin/testdso.${dso}'/>"
	content += `
	<request >
		<table name='BEGIN'>
			<chain  action='continue' >
				<mark_test  test></mark_test>
			</chain>
		</table>		
	</request>
	<response action='allow' >
		<table name='BEGIN'>
			<chain  action='continue' >
				<acl_reg_param  param='^no-cache' nc='1'></acl_reg_param>
				<mark_response_flag   flagvalue='nocache,'></mark_response_flag>
			</chain>
		</table>
	</response>
<vh name='dso' doc_root='www'  inherit='on' app='1'>
	<map path='/' extend='dso:testdso:async_test' confirm_file='0' allow_method='*'/>
	<host>dso.localtest.me</host>
</vh>
</config>
`
	kangle.CreateExtConfig("dso", content)
	return nil
}
func (d *dso) Clean() {
	kangle.CleanExtConfig("dso")
}
func init() {
	s := &dso{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "dso"
	s.AddCase("upstream", "dso的upstream", check_upstream)
	s.AddCase("filter", "dso的filter", check_filter)
	s.AddCase("bc_chunk", "before cache chunk", check_before_cache_chunk)
	s.AddCase("http_10_chunk", "http/1.0 meet chunked", check_http10_chunk)
	s.AddCase("websocket", "dso websocket", test_dso_websocket)
	s.AddCase("sendfile", "sendfile test", check_sendfile)
	suite.Register(s)
}
