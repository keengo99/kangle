package dso_suite

import (
	"fmt"
	"runtime"
	"test_server/kangle"
	"test_server/suite"
)

type dso struct {
	suite.Suite
}

func (this *dso) Init() error {
	fmt.Printf("dso init\n")
	content := "<!--#start 300-->\r\n<config>\r\n"

	if runtime.GOOS == "windows" {
		content += "<dso_extend name='testdso' filename='bin/testdso.dll'/>"
	} else {
		content += "<dso_extend name='testdso' filename='bin/testdso.so'/>"
	}
	content += `
	<request >
		<table name='BEGIN'>
			<chain  action='continue' >
				<mark_test  test></mark_test>
			</chain>
		</table>
	</request>
	<!--vh start-->
	<vh name='default' doc_root='www'  inherit='on' app='1'>
	<map path='/test_dso' extend='dso:testdso:async_test' confirm_file='0' allow_method='*'/>
	<host>*</host>
	</vh>
</config>
`

	kangle.CreateExtConfig("dso", content)
	return nil
}
func (this *dso) Clean() {
	kangle.CleanExtConfig("dso")
}
func init() {
	s := &dso{}
	s.Name = "dso"
	s.AddCase("upstream", "dso的upstream", check_upstream)
	s.AddCase("filter", "dso的filter", check_filter)
	suite.Register(s)
}
