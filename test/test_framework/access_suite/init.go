package access_suite

import (
	"test_framework/config"
	"test_framework/kangle"
	"test_framework/server"
	"test_framework/suite"
)

var CONFIG_FILE_NAME = "access"

type access struct {
	suite.Suite
}

func (a *access) Init() error {
	server.Handle("/footer", handleFooter)
	server.Handle("/access/header", handle_header)
	server.Handle("/access/auth", handle_auth)
	server.Handle("/access/digest", handle_auth)
	server.Handle("/status-code", handleStatusCode)
	server.Handle("/filters/replace-content", handleLegacyFilter)
	server.Handle("/filters/replace-url", handleLegacyFilter)
	server.Handle("/filters/replace-location", handleLegacyFilter)
	server.Handle("/filters/content-deny", handleLegacyFilter)
	server.Handle("/filters/status", handleLegacyFilter)
	server.Handle("/filters/range", handleLegacyFilter)
	server.Handle("/filters/url-range/", handleLegacyFilter)
	server.Handle("/filters/guest-cache", handleGuestCache)
	server.Handle("/builtin-filter/param", handleBuiltinFilter)
	server.Handle("/builtin-filter/count", handleBuiltinFilter)
	server.Handle("/builtin-filter/file", handleBuiltinFilter)

	config := `<!--#start 300-->\r\n
<config>
	<request>
		<table name='BEGIN'>
			<chain action='drop'>
				<acl_path path='/drop-no-response'/>
			</chain>
		</table>
	</request>
<vh name='access' doc_root='www'  inherit='on' app='1' access='-'>	
	<map path='/' extend='server:upstream' confirm_file='0' allow_method='*'/>
	<request>
		<table name='BEGIN'>
			<chain action='deny'>
				<acl_path path='/builtin-filter/param'/>
				<mark_param name='^blocked$' value='^yes$' get='1' post='0'/>
			</chain>
			<chain action='deny'>
				<acl_path path='/builtin-filter/count'/>
				<mark_param_count max_count='1' get='1' post='0'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/builtin-filter/file'/>
				<mark_post_file filename='\.blocked$' />
			</chain>
			<chain action='allow'>
				<acl_path path='/status-code'/>
				<acl_header header='X-Test-Status-Code' val='^1$'/>
				<mark_status_code code='451'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/url-range/*'/>
				<mark_url_range range_from='url-range/([0-9]+)-([0-9]+)' range_to=''/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/guest-cache'/>
				<mark_flag guest='1'/>
			</chain>
			<chain  action='continue' >
					<mark_rewrite prefix='/' path='^rw(.*)$' dst='/wr$1' internal='0' nc='1' qsa='1' code='302'></mark_rewrite>
			</chain>
			<chain  action='continue' >
				<acl_path  path='/redirect'></acl_path>
				<mark_redirect dst='http://redirect.localtest.me:9999/' code='302'></mark_redirect>
			</chain>
			<chain  action='continue' >
				<mark_remove_header   attr='x-header' val='a' ></mark_remove_header>
			</chain>
			<chain  action='continue' >
				<mark_replace_header   attr='x-header' val='(.*)c(.*)' replace='$1d$2'></mark_replace_header>
			</chain>
			<chain  action='continue' >
				<acl_path path='/auth_load_failed'/>
				<mark_auth   file='must_not_exsit_file' crypt_type='plain' auth_type='Basic' realm='kangle' require='*' failed_deny='1'></mark_auth>
			</chain>
			<chain  action='continue' >
				<acl_path path='/access/auth'/>
				<mark_auth   file='auth.txt' crypt_type='plain' auth_type='Basic' realm='kangle' require='user' failed_deny='1'></mark_auth>
			</chain>
			<chain  action='continue' >
				<acl_path path='/access/digest'/>
				<mark_auth   file='auth.txt' crypt_type='plain' auth_type='Digest' realm='kangle' require='user' failed_deny='1'></mark_auth>
			</chain>
		</table>
	</request>
	<response >
		<table name='BEGIN'>
			<chain action='continue'>
				<acl_path path='/filters/replace-content'/>
				<mark_replace_content content='foo([0-9]+)' replace='bar$1' buffer='1m'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/replace-url'/>
				<mark_replace_url src='^http://old\.test/(.*)$' dst='https://new.test/$1'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/replace-location'/>
				<mark_replace_url src='^http://old\.test/(.*)$' dst='https://new.test/$1'/>
			</chain>
			<chain action='deny'>
				<acl_path path='/filters/content-deny'/>
				<mark_content buffer='1m'><![CDATA[blocked-value]]></mark_content>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/status'/>
				<mark_status_code code='418'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/range'/>
				<mark_fix_header header='%48%44%52'/>
			</chain>
			<chain action='continue'>
				<acl_path path='/filters/guest-cache'/>
				<mark_guest_cache max_age='60' skip_set_cookie='1'/>
			</chain>
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
	s.AddCase("header", "header测试", check_header)
	s.AddCase("auth", "http auth", check_http_auth)
	s.AddCase("status_code", "HTTP状态码标记测试", check_status_code)
	s.AddCase("drop", "请求控制无响应断开连接测试", check_drop)
	s.AddCase("builtin_filter", "内置过滤规则测试", check_builtin_filter)
	s.AddCase("legacy_filters", "旧版回应过滤规则兼容测试", check_legacy_filters)
	suite.Register(s)
}
