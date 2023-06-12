package webdav_suite

import (
	"test_framework/config"
	"test_framework/kangle"
	"test_framework/suite"
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
		<host>` + config.GetLocalhost("webdav") + `</host>
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
	s.AddCase("client", "使用webdav client测试", check_client)
	suite.Register(s)
}
