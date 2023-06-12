package vh_suite

import (
	"net/http"
	"test_framework/config"
	"test_framework/kangle"
	"test_framework/suite"
)

type vh struct {
	suite.Suite
}

func (a *vh) Init() error {
	kangle.CreateExtConfig("100", `<!--#start 100 -->
	<config>
		<vh name='vh_test' doc_root='www'  inherit='on' app='1'>
			<host>`+config.GetLocalhost("vh")+`</host>
		</vh>
	</config>	
	`)
	kangle.Reload("ext|100.xml_")
	http.HandleFunc("/map_path/test", handle_map)
	http.HandleFunc("/map_file/test.map_file", handle_map)
	http.HandleFunc("/map_path/alias-x-sendfile", handle_alias_x_sendfile)
	return nil
}
func (a *vh) Clean() {
	kangle.CreateExtConfig("100", `<!--#start 100 --><config></config>`)
	kangle.CreateExtConfig("10", `<!--#start 10 --><config></config>`)
	kangle.Reload("ext|100.xml_")
	kangle.Reload("ext|10.xml_")
}
func init() {
	s := &vh{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "vh"
	suite.Register(s)
	s.AddCase("index", "测试vh继承index", check_index)
	s.AddCase("map", "测试vh继承map", check_map)
	s.AddCase("alias", "测试alias", check_alias)
	s.AddCase("error", "测试error code", check_error_code)
}
