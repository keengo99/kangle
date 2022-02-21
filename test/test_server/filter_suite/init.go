package filter_suite

import (
	"test_server/kangle"
	"test_server/server"
	"test_server/suite"
)

var CONFIG_FILE_NAME = "filter"

type filter struct {
	suite.Suite
}

func (this *filter) Init() error {
	server.Handle("/footer", handleFooter)
	config := `
<config>
		<response >
		<table name='BEGIN'>
			<chain  action='continue' >
				<acl_path  path='/footer'></acl_path>
				<mark_footer ><![CDATA[footer]]></mark_footer>
			</chain>
		</table>
	</response>
</config>`
	return kangle.CreateExtConfig(CONFIG_FILE_NAME, config)

}
func (this *filter) Clean() {
	kangle.CleanExtConfig(CONFIG_FILE_NAME)
}
func init() {
	s := &filter{}
	s.Name = "filter"
	s.AddCase("footer", "footer测试", check_footer)
	suite.Register(s)
}
