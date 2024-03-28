package base_suite

import (
	"test_framework/config"
	"test_framework/kangle"
	"test_framework/server"
	"test_framework/suite"
)

type base struct {
	suite.Suite
}

var CONFIG_FILE_NAME = "base"

func (b *base) Init() error {
	server.Handle("/100_continue", handle_100_continue)
	server.Handle("/post_chunk_trailer", HandlePostChunkTrailer)
	server.Handle("/chunk_trailer", HandleChunkUpstreamTrailer)
	server.Handle("/chunk_trailer_1", HandleHttp1ChunkUpstreamTrailer)
	server.Handle("/obs_fold", handle_obs_fold)
	server.Handle("/chunk", HandleChunkUpstream)
	server.Handle("/split_response", HandleSplitResponse)
	server.Handle("/chunk_post", HandleChunkPost)
	server.Handle("/etag", HandleEtag)
	server.Handle("/dynamic_etag", handle_dynamic_etag)
	server.Handle("/etag_last_modified", handle_etag_last_modified)
	server.Handle("/br", HandleBr)
	server.Handle("/gzip", HandleGzip)
	server.Handle("/gzip_br", HandleGzipBr)
	server.Handle("/chunk_compress", handle_chunk_compress)

	server.Handle("/dynamic", HandleDynamic)
	server.Handle("/no-cache", HandleNoCache)
	server.Handle("/hole", HandleHole)
	server.Handle("/miss_status_string", HandleMissStatusString)
	server.Handle("/vary", HandleVary)
	server.Handle("/read_hup", HandleReadHup)
	server.Handle("/websocket", HandleWebsocketHijack)
	server.Handle("/websocket2", HandleWebsocket)
	server.Handle("/broken_cache", HandleBrokenCache)
	server.Handle("/upstream_http_protocol", HandleUpstreamHttpProtocol)
	server.Handle("/flush", HandleFlush)
	str := `<!--#start 200-->
<config>
	<request>
		<table name='BEGIN'>			
			<chain  action='continue' >
				<mark_flag  x_cache='1' age='1' via='1'></mark_flag>
			</chain>
			<chain  action='return' >
				<acl_path  path='/stub_status'></acl_path>
				<mark_stub_status  ></mark_stub_status>
			</chain>
			<chain  action='server:localhost_proxy' >
				<acl_self_port >9800</acl_self_port>
			</chain>
			<chain  action='server:localhost_https' >
				<acl_self_port >9900</acl_self_port>
			</chain>
			<chain  action='server:localhost' >
				<acl_self_port >9902</acl_self_port>
			</chain>
			<chain  action='server:upstream_h2' >
				<acl_path>/upstream/h2/*</acl_path>
			</chain>
			<chain  action='server:upstream_h2c' >
				<acl_path>/upstream/h2c/*</acl_path>
			</chain>
			<chain  action='server:upstream_ssl' >
				<acl_path>/upstream/ssl/*</acl_path>
			</chain>
			<chain  action='server:upstream_h2' >
				<acl_self_port >9801</acl_self_port>
			</chain>
		</table>
	</request>
	<response action='allow' >
		<table name='BEGIN'>
			<chain  action='continue' >
				<acl_path  path='/force_cache/*'></acl_path>
				<mark_cache_control   max_age='10' force='1'></mark_cache_control>
			</chain>
		</table>
	</response>
	<!--vh start-->
	<vh name='base' doc_root='www'  inherit='on' app='1'>
		<index id='100' file='index.html'/>
		<map path='/static' extend='default' confirm_file='0' allow_method='*'/>
		<map path='/fastcgi' extend='cmd:fastcgi' confirm_file='0' allow_method='*'/>
		<map path='/' extend='server:`
	if config.Cfg.UpstreamSsl {
		str += "upstream_ssl"
	} else {
		str += "upstream"
	}
	str += `' confirm_file='0' allow_method='*'/>
		<host>127.0.0.1</host>
		<host>localhost</host>
	</vh>
	<vh name='apache' doc_root='www' inherit='on' htaccess='htaccess.txt' app='1' >
		<host>` + config.GetLocalhost("apache") + `</host>
		<host>` + config.GetLocalhost("apache2") + `</host>
	</vh>
	</config>`
	kangle.CreateExtConfig("10", "<!--# start 10 -->\r\n<config></config>")
	kangle.CreateExtConfig("100", "<!--# start 100 -->\r\n<config></config>")
	return kangle.CreateExtConfig(CONFIG_FILE_NAME, str)
}
func (b *base) Clean() {
	kangle.CleanExtConfig(CONFIG_FILE_NAME)
}
func init() {

	s := &base{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "base"
	suite.Register(s)
	s.AddCase("http2https", "发送http到https端口", check_http2https)
	s.AddCase("etag", "etag支持", check_etag)
	s.AddCase("etag_last_modified", "测试etag和Last-Modified同时存在情况", check_cache_etag_last_modified)
	s.AddCase("cache_etag", "测试etag变化,cache要正确。", check_cache_etag)
	s.AddCase("dynamic", "动态内容使用etag缓存", check_dynamic_content)
	s.AddCase("bigobj_md5", "大物件缓存，检测md5", check_bigobj_md5)
	s.AddCase("br_unknow", "br和unknow的encoding", check_br_unknow_encoding)
	s.AddCase("encoding_pri", "不同顺序请求encoding的优先级", check_encoding_priority)
	s.AddCase("compress", "gzip/br压缩", check_compress)
	s.AddCase("chunk_compress", "块压缩", check_chunk_compress)
	s.AddCase("bigobj", "大物件缓存最简单缓存", check_big_object)
	s.AddCase("weak_etag", "weak etag测试", check_weak_etag_range)
	s.AddCase("bigobj_range", "大物件简单部分缓存", check_simple_range)
	s.AddCase("bigobj_multi_miss", "大文件缓存，多段缺失", check_multi_miss)
	s.AddCase("bigobj_upstream_error", "大文件缓存,源异常不可用", check_bigobj_upstream_error)
	s.AddCase("if_range_forward", "if-range转发", check_if_range_forward)
	s.AddCase("if_range_local", "if-range本地", check_if_range_local)
	s.AddCase("not_get_cache", "not GET cache", check_not_get_cache)
	s.AddCase("not_200_cache", "not 200 cache", check_not_200_cache)
	s.AddCase("last_range", "大物件尾部缓存命中", check_last_range)
	s.AddCase("sbo_not_enough_bug", "", check_sbo_not_enough_bug)
	s.AddCase("nochange_if_range", "内容不变，客户端发送if-range支持", check_nochange_if_range)
	s.AddCase("change_if_range", "内容变化,客户端发送if-range", check_change_if_range)
	s.AddCase("nochange_first_part", "内容不变，前面部分缓存", check_nochange_first_part)
	s.AddCase("nochange_first_hit", "内容不变，前面全部缓存", check_nochange_first_hit)
	s.AddCase("change_first_miss", "内容变化，前面miss", check_change_first_miss)
	s.AddCase("change_first_hit", "内容变化，前面hit", check_change_first_hit)
	s.AddCase("nochange_first_miss", "内容不变，前面miss", check_nochange_first_miss)
	s.AddCase("nochange_middle_hit", "内容不变，中间命中", check_nochange_middle_hit)
	s.AddCase("bigobj_client_if_range", "大方件缓存,客户有不同的if-range", check_bigobj_client_if_range)
	s.AddCase("miss_status_string", "上流有status_code但缺失status信息", check_miss_status_string)
	s.AddCase("http_1_1_pipe_line", "http/1.1的pipe line支持", check_http_1_1_pipe_line)
	s.AddCase("chunk_post", "form表单chunk方式上传", check_chunk_post)
	s.AddCase("chunk_trailer", "支持trailer的chunk", check_chunk_trailer)
	s.AddCase("split_response", "上流分割回应", check_split_response)
	s.AddCase("chunk_upstream", "上流chunk回应", check_chunk_upstream)
	s.AddCase("vary", "vary支持", check_vary)
	s.AddCase("proxy", "proxy协议", check_proxy_port)
	//s.AddCase("extworker", "extworker", check_extworker)
	//s.AddCase("read_hup", "read_hup测试(配合dso以及linux下)", check_read_hup)
	s.AddCase("websocket", "websocket", test_websocket)
	s.AddCase("websocket_over_h2", "websocket over h2", test_websocket_over_h2)
	s.AddCase("websocket_no_h2", "http1 websocket和上游禁止h2", test_upstream_disable_h2_websocket)
	s.AddCase("head", "head method", test_head_method)
	s.AddCase("options", "options method", test_options_method)
	s.AddCase("broken_no_cache", "连接中断不能缓存", check_broken_no_cache)
	s.AddCase("disk_cache", "磁盘缓存swap out/in", check_disk_cache)
	s.AddCase("upstream_http_protocol", "测试上游http协议解析", check_upstream_http_protocol)
	s.AddCase("fastcgi", "fastcgi协议测试", check_fastcgi)
	s.AddCase("obs_fold", "obs_fold测试", check_obs_fold)
	s.AddCase("100_continue", "100-continue测试", check_100_continue)
	s.AddCase("sub_status", "sub_status", test_stub_status)
	s.AddCase("htaccess", "check apache htaccess", check_htaccess)
	s.AddCase("directory", "check directory", check_directory)
	s.AddCase("config", "check config reload", check_config)
	s.AddCase("flush", "upstream flush", check_flush)
	s.AddCase("bug", "bug", check_bug)
}
