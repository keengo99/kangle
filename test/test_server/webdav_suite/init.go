package webdav_suite

import (
	"test_server/kangle"
	"test_server/suite"
)

var CONFIG_FILE_NAME = "webdav"

type webdav struct {
	suite.Suite
}

func (this *webdav) Init() error {

	content := "<!--#start 300-->\r\n<config>\r\n"
	content += `
	<api name='webdav' file='bin/webdav.${dso}' life_time='60' max_error_count='5'>
	</api>
	<!--vh start-->
	<vh name='webdav' doc_root='www'  inherit='on' app='1'>
		<map path='/dav' extend='api:webdav' confirm_file='0' allow_method='*'/>
		<host>webdav.test</host>
	</vh>
</config>
`
	return kangle.CreateExtConfig(CONFIG_FILE_NAME, content)

}
func (this *webdav) Clean() {
	kangle.CleanExtConfig(CONFIG_FILE_NAME)
}
func init() {
	s := &webdav{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "webdav"
	s.AddCase("options", "测试OPTIONS", check_options)
	suite.Register(s)
}
