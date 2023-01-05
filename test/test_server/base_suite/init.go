package base_suite

import (
	"test_server/common"
	"test_server/config"
	"test_server/kangle"
	"test_server/server"
	"test_server/suite"
)

type base struct {
	suite.Suite
}

var CONFIG_FILE_NAME = "base"

func (this *base) Init() error {
	createRange(1024)

	server.Handle("/post_chunk_trailer", HandlePostChunkTrailer)
	server.Handle("/chunk_trailer", HandleChunkUpstreamTrailer)
	server.Handle("/chunk_trailer_1", HandleHttp1ChunkUpstreamTrailer)

	server.Handle("/chunk", HandleChunkUpstream)
	server.Handle("/split_response", HandleSplitResponse)
	server.Handle("/chunk_post", HandleChunkPost)
	server.Handle("/etag", HandleEtag)
	server.Handle("/br", HandleBr)
	server.Handle("/gzip", HandleGzip)
	server.Handle("/gzip_br", HandleGzipBr)
	server.Handle("/range", HandleRange)
	server.Handle("/dynamic", HandleDynamic)
	server.Handle("/no-cache", HandleNoCache)
	server.Handle("/hole", HandleHole)
	server.Handle("/miss_status_string", HandleMissStatusString)
	server.Handle("/vary", HandleVary)
	server.Handle("/read_hup", HandleReadHup)
	server.Handle("/websocket", HandleWebsocket)
	server.Handle("/broken_cache", HandleBrokenCache)
	server.Handle("/upstream_http_protocol", HandleUpstreamHttpProtocol)
	str := `<!--#start 200-->
<config>
	<!-- 9998开启proxy protocol端口 -->
	<listen ip='127.0.0.1' port='9998P' type='http' />
	<!-- 9800中转到kangle的9998端口 -->
	<listen ip='127.0.0.1' port='9800' type='tcp' />
	<!-- 9801中转连接上游h2(test_server的4412端口) -->
	<listen ip='127.0.0.1' port='9801' type='http' />
	<!-- 9900中转连接kangle的h2(kangle的9443端口) -->
	<listen ip='127.0.0.1' port='9900' type='http' />
	<!-- 9902 https中转上上游http(kangle的9999) -->
	<listen ip='127.0.0.1' port='9902' type='https' certificate='etc/server.crt' certificate_key='etc/server.key' http2='1' />
	<gzip only_gzip_cache='0' min_gzip_length='1' gzip_level='5' br_level='5'/>
	<cache default='1' max_cache_size='256k' max_bigobj_size='1g' memory='1G' disk='1g' cache_part='1' refresh_time='30'/>
	`
	str += "<cmd name='fastcgi' proto='fastcgi' file='bin/test_child" + common.ExeExtendFile() + " f 9005' port='9005' type='sp' param='' life_time='280' idle_time='280'>"
	str += `
	</cmd>
	<server name='localhost_proxy' proto='proxy' host='127.0.0.1' port='9998' life_time='5' />
	<server name='localhost' proto='http' host='127.0.0.1' port='9999' life_time='5' />
	<server name='localhost_https' proto='http' host='127.0.0.1' port='9943sp' life_time='2' />
	<server name='upstream' host='127.0.0.1' port='4411' proto='http' life_time='10'/>
	<server name='upstream_h2c' host='127.0.0.1' port='4411h' proto='http' life_time='10'/>
	<server name='upstream_ssl' host='127.0.0.1' port='4412s' proto='http' life_time='10'/>	
	<server name='upstream_h2' host='127.0.0.1' port='4412sp' proto='http' life_time='10'/>	
	<request>
		<table name='BEGIN'>
			<chain  action='continue' >
				<mark_flag  x_cache='1' age='1' ></mark_flag>
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
			<chain  action='server:upstream_h2' >
				<acl_self_port >9801</acl_self_port>
			</chain>
		</table>
	</request>
	<!--vh start-->
	<vh name='base' doc_root='www'  inherit='on' app='1'>		
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
	</config>`
	return kangle.CreateExtConfig(CONFIG_FILE_NAME, str)
}
func (this *base) Clean() {
	kangle.CleanExtConfig(CONFIG_FILE_NAME)
}
func init() {

	s := &base{}
	s.CasesMap = make(map[string]*suite.Case)
	s.Name = "base"
	suite.Register(s)
	s.AddCase("http2https", "发送http到https端口", check_http2https)
	s.AddCase("etag", "etag支持", check_etag)
	s.AddCase("dynamic", "动态内容使用etag缓存", check_dynamic_content)
	s.AddCase("br_unknow", "br和unknow的encoding", check_br_unknow_encoding)
	s.AddCase("encoding_pri", "不同顺序请求encoding的优先级", check_encoding_priority)
	s.AddCase("compress", "gzip/br压缩", check_compress)
	s.AddCase("bigobj", "大物件缓存最简单缓存", check_big_object)
	s.AddCase("bigobj_range", "大物件简单部分缓存", check_simple_range)
	s.AddCase("if_range_forward", "if-range转发", check_if_range_forward)
	s.AddCase("last_range", "大物件尾部缓存命中", check_last_range)
	s.AddCase("sbo_not_enough_bug", "", check_sbo_not_enough_bug)
	s.AddCase("nochange_if_range", "内容不变，客户端发送if-range支持", check_nochange_client_if_range)
	s.AddCase("nochange_first_part", "内容不变，前面部分缓存", check_nochange_first_part)
	s.AddCase("nochange_first_hit", "内容不变，前面全部缓存", check_nochange_first_hit)
	s.AddCase("change_first_miss", "内容变化，前面miss", check_change_first_miss)
	s.AddCase("change_first_hit", "内容变化，前面hit", check_change_first_hit)
	s.AddCase("nochange_middle_hit", "内容不变，中间命中", check_nochange_middle_hit)
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
	s.AddCase("websocket_h2", "websocket和h2", test_upstream_h2_websocket)
	s.AddCase("head", "head method", test_head_method)
	s.AddCase("broken_no_cache", "连接中断不能缓存", check_broken_no_cache)
	s.AddCase("disk_cache", "磁盘缓存swap out/in", check_disk_cache)
	s.AddCase("upstream_http_protocol", "测试上游http协议解析", check_upstream_http_protocol)
	s.AddCase("fastcgi", "fastcgi协议测试", check_fastcgi)
	//s.AddCase("h3_method", "h3 method", test_h3_method)
}
