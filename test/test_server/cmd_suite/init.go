package cmd_suite

import (
	"fmt"
	"test_server/common"
	"test_server/kangle"
	"test_server/suite"
)

type cmd struct {
	suite.Suite
}

func (this *cmd) Init() error {
	fmt.Printf("cmd init\n")
	content := `<!--#start 300-->
<config>
	<cmd name='http_mp' proto='http' file='bin/test_child` + common.ExeExtendFile() + ` h' type='mp' param='' life_time='60' idle_time='120'>
	</cmd>
	<cmd name='fastcgi_mp' proto='fastcgi' file='bin/test_child` + common.ExeExtendFile() + ` f' type='mp' param='' life_time='60' idle_time='120'>
	</cmd>
	<cmd name='http_sp' proto='http' file='bin/test_child` + common.ExeExtendFile() + ` h 9003' port='9003' type='sp' param='' life_time='60' idle_time='120'>
	</cmd>
	<cmd name='fastcgi_sp' proto='fastcgi' file='bin/test_child` + common.ExeExtendFile() + ` f 9004' port='9004' type='sp' param='' life_time='60' idle_time='120'>
	</cmd>
	<vh name='cmd' >
		<map file_ext='fastcgi_mp' extend='cmd:fastcgi_mp' confirm_file='0' allow_method='*'/>
		<map file_ext='fastcgi_sp' extend='cmd:fastcgi_sp' confirm_file='0' allow_method='*'/>
		<map file_ext='http_mp' extend='cmd:http_mp' confirm_file='0' allow_method='*'/>
		<map file_ext='http_sp' extend='cmd:http_sp' confirm_file='0' allow_method='*'/>
		<host>cmd.test</host>
	</vh>
</config>
`
	kangle.CreateExtConfig("cmd", content)
	return nil
}
func (this *cmd) Clean() {
	kangle.CleanExtConfig("cmd")
}
func init() {
	s := &cmd{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "cmd"
	s.AddCase("fastcgi_mp", "多进程fastcgi", check_mp_fastcgi)
	s.AddCase("fastcgi_sp", "单进程fastcgi", check_sp_fastcgi)
	s.AddCase("http_mp", "多进程http", check_mp_http)
	s.AddCase("http_sp", "单进程http", check_sp_http)
	//s.AddCase("fastcgi", "服务器扩展fastcgi", check_fastcgi)
	suite.Register(s)
}
