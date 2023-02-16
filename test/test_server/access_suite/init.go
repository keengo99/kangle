package access_suite

import (
	"test_server/config"
	"test_server/kangle"
	"test_server/server"
	"test_server/suite"
)

var CONFIG_FILE_NAME = "access"

type access struct {
	suite.Suite
}

func (a *access) Init() error {
	server.Handle("/footer", handleFooter)
	config := `<!--#start 300-->\r\n
<config>
<dso_extend name='filter' filename='bin/filter.${dso}'/>
<vh name='access' doc_root='www'  inherit='on' app='1' access='-'>	
	<map path='/' extend='server:upstream' confirm_file='0' allow_method='*'/>
	<request>
		<table name='BEGIN'>
			<table name='BEGIN'>
				<chain  action='continue' >
					<mark_rewrite prefix='/' path='^rw(.*)$' dst='/wr$1' internal='0' nc='1' qsa='1' code='302'></mark_rewrite>
				</chain>
			</table>
			<chain  action='continue' >
				<acl_path  path='/redirect'></acl_path>
				<mark_redirect dst='http://redirect.localtest.me:9999/' code='302'></mark_redirect>
			</chain>
		</table>
	</request>
	<response >
		<table name='BEGIN'>
			<chain  action='continue' >
				<acl_path  path='/footer'></acl_path>
				<mark_footer ><![CDATA[footer]]></mark_footer>
			</chain>
		</table>
	</response>
	<host>` + config.GetLocalhost("access") + `</host>
</vh>
</config>`
	return kangle.CreateExtConfig(CONFIG_FILE_NAME, config)

}
func (a *access) Clean() {
	kangle.CleanExtConfig(CONFIG_FILE_NAME)
}
func init() {
	s := &access{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "access"
	s.AddCase("footer", "footer测试", check_footer)
	s.AddCase("redirect", "重定向测试", check_redirect)
	s.AddCase("rewrite", "重写测试", check_rewrite)
	suite.Register(s)
}
