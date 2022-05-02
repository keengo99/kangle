#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "global.h"

#include "KHttpManage.h"
#include "kthread.h"
#include "kselector_manager.h"
#include "cache.h"
#include "log.h"
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <ctype.h>
#include <time.h>
#include "kmalloc.h"
#include "lib.h"
#include "KHttpRequest.h"
#include "KAcserverManager.h"
#include "KSingleAcserver.h"
#include "KWriteBackManager.h"
#include "KConfigParser.h"
#include "KHttpObjectHash.h"
#include "KConfigBuilder.h"
#include "KAccess.h"
#include "utils.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"
#include "KHttpServerParser.h"
#include "KVirtualHostManage.h"
#include "iconv.h"
#include "KProcessManage.h"
#include "KLogHandle.h"
#include "KUrlParser.h"
#include "KDsoExtendManage.h"
#include "KCache.h"
#include "kaddr.h"
#include "HttpFiber.h"
#include "KPreRequest.h"
#include "extern.h"
#include "lang.h"
#include "kmd5.h"
using namespace std;
string get_connect_per_ip()
{
	return "";
}
KTHREAD_FUNCTION check_autoupdate(void *param);
void get_url_info(KSink* rq, KStringBuf &s) {
#ifdef HTTP_PROXY
	if (rq->data.meth == METH_CONNECT) {
		s << "CONNECT ";
		if (rq->data.raw_url.host) {
			s << rq->data.raw_url.host << ":" << rq->data.raw_url.port;
		}
		return;
	}
#endif//}}
	rq->data.raw_url.GetUrl(s, true);
	return;
}

bool getConnectionTr(KSink *rq, KConnectionInfoContext *ctx)
{
	if (conf.max_connect_info > 0 && ctx->total_count > (int)conf.max_connect_info) {
		return false;
	}
	KSubVirtualHost* svh = static_cast<KSubVirtualHost*>(rq->data.opaque);
	if (ctx->vh != NULL && (svh==NULL || strcmp(svh->vh->name.c_str(), ctx->vh) != 0)) {
		return true;
	}
	ctx->total_count++;
	ctx->s << "rqs.push(new Array(";
	ctx->s << "'";
	if (ctx->translate) {
		char ips[MAXIPLEN];
		sockaddr_i *addr = rq->get_peer_addr();
		ksocket_sockaddr_ip(addr, ips, MAXIPLEN);
		ctx->s << "rq=" << (void *)rq;
		ctx->s << ",peer=" << ips << ":" << ksocket_addr_port(addr);
	}
	ctx->s << "','" << rq->get_client_ip();
	ctx->s << "','" << (kgl_current_sec - rq->data.begin_time_msec / 1000);
	ctx->s << "','";
#if 0
	if (ctx->translate) {
		ctx->s << klang[rq->getState()];
	} else {
		ctx->s << rq->getState();
	}
#endif
	ctx->s << "','";
	ctx->s << rq->data.meth;
	ctx->s << "','";
	KStringBuf url;
	get_url_info(rq, url);
	if (url.getSize() > 0 && strchr(url.getString(),'\'')==NULL) {
		ctx->s << url.getString();
	}
	ctx->s << "','";
	if (!ctx->translate) {
		sockaddr_i self_addr;
		char ips[MAXIPLEN];
		rq->get_self_addr(&self_addr);
		ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
		ctx->s << ips << ":" << ksocket_addr_port(&self_addr);
	}
	ctx->s << "','";
	KHttpHeader* referer = rq->data.FindHeader(kgl_expand_string("Referer"));
	if (referer) {
		ctx->s << KXml::encode(referer->val);
	}
	ctx->s << "','";
	ctx->s << (int)rq->data.http_major;
	ctx->s << "));\n";
	return true;
}
bool kconnection_info_iterator(void *ctx, kselector *selector, kselectable *st)
{
	KConnectionInfoContext *cn_ctx = (KConnectionInfoContext *)ctx;
	if (st->data == NULL || !KBIT_TEST(st->st_flags,STF_OPAQUE_SERVER)) {
		return true;
	}
	if (!KBIT_TEST(st->st_flags, STF_OPAQUE_HTTP2)) {
		return getConnectionTr((KSink *)st->data,cn_ctx);
	}
#ifdef ENABLE_HTTP2
	KHttp2 *http2 = (KHttp2 *)st->data;
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	kassert(!http2->IsClientModel());
	if (http2->IsClientModel()) {
		return true;
	}
#endif//}}
	KHttp2Node **nodes = http2->GetAllStream();
	for (int k = 0; k < kgl_http_v2_index_size(); k++) {
		KHttp2Node *node = nodes[k];
		while (node) {
			if (node->stream == NULL || node->stream->request == NULL) {
				node = node->index;
				continue;
			}
			KSink *rq = node->stream->request;
			node = node->index;			
			if (!getConnectionTr(rq, cn_ctx)) {
				return false;
			}
		}
	}
#endif
	return true;
}
string endTag() {
	std::stringstream s;
	if(need_reboot_flag){
		s << "<font color='red'>" << klang["need_reboot"] << "</font> <a href=\"javascript:if(confirm('really reboot')){ window.parent.location='/reboot';}\">" << klang["reboot"] << "</a>";
	}
	s << "<hr>";
	if (*conf.server_software) {
		s << "<!-- ";
	}
	s << "<center>Powered by <a href='http://www.kangleweb.net/' target='_blank'>"
		  << PROGRAM_NAME << "/" << VERSION << "</a>(" << getServerType() << "), "
		  << LANG_COPY_RIGHT_STR << "</center>";
	if (*conf.server_software) {
		s << " -->";
	}
	return s.str();
}
bool killProcess(KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	conf.gam->killAllProcess(vh);
#endif
	return true;
}
bool killProcess(std::string process,std::string user,int pid)
{

	char *name = xstrdup(process.c_str());
	if (name == NULL) {
		return false;
	}
	char *p = strchr(name, ':');
	if (p == NULL) {
		xfree(name);
		return false;
	}
	*p = '\0';
	p++;
	if (strcasecmp(name, "api") == 0) {
		spProcessManage.killProcess(user.c_str(),pid);
	} else {
	#ifdef ENABLE_VH_RUN_AS
		KCmdPoolableRedirect *rd = conf.gam->refsCmdRedirect(p);
		if (rd) {
			KProcessManage *pm = rd->getProcessManage();
			if (pm) {
				pm->killProcess(user.c_str(),pid);
			}
			rd->release();
		}
	#endif
	}
	xfree(name);
	return true;
}
bool changeAdminPassword(KUrlValue *url,std::string &errMsg)
{
		stringstream s;
		string admin_passwd = url->get("admin_passwd");
		string admin_user = url->get("admin_user");
		int auth_type = KHttpAuth::parseType(url->get("auth_type").c_str());
		if (admin_passwd.size() == 0) {
			if (auth_type != conf.auth_type) {
				errMsg = "change auth_type must reset password.please enter admin password";
				return false;
				//return sendErrPage(
				//		"change auth_type must reset password.please enter admin password");
			}
			if (auth_type == AUTH_DIGEST && conf.admin_user != admin_user) {
				errMsg = "use Digest auth when you change admin user you must reset the password.Please enter admin password";
				return false;
				//	return sendErrPage(
			//			"use Digest auth when you change admin user you must reset the password.Please enter admin password");
			}
		}
		conf.admin_user = admin_user;
		if (admin_passwd.size() > 0) {
			conf.admin_passwd = admin_passwd;
			conf.passwd_crypt = CRYPT_TYPE_PLAIN;
		}
		conf.auth_type = auth_type;
		change_admin_password_crypt_type();
		explode(url->get("admin_ips").c_str(), '|', conf.admin_ips);
		//		m_config.setValue("admin_user",conf.admin_user.c_str());
	/*	for (i = 0; i < conf.admin_ips.size(); i++) {
			s << conf.admin_ips[i] << "|";
		}
		*/
		return true;
}

string chanagePasswordForm() {
	return "<html><LINK href=/main.css type='text/css' rel=stylesheet><body><form action=chanage_password method=get>old password:<input type=password name=old_password><br>new password:<input type=password name=new_password><br>retype new password:<input type=password name=re_new_password><br><input type=submit value=submit></form></body></html>";
}
bool KHttpManage::runCommand() {
	string cmd = getUrlValue("cmd");

	if (cmd == "flush_log") {
#ifndef HTTP_PROXY
		KHttpServerParser logParser;
		string configFile = conf.path;
		configFile += VH_CONFIG_FILE;
		//conf.gvm->clear();
		logParser.parse(configFile);
#endif
		return sendHttp("200");
#ifdef MALLOCDEBUG
	} else if (cmd == "dump_memory") {
		int min_time = atoi(getUrlValue("min_time").c_str());
		string max_time_str = getUrlValue("max_time");
		int max_time = -1;
		if (max_time_str.size() > 0) {
			max_time = atoi(max_time_str.c_str());
		}
		dump_memory_leak(min_time, max_time);
		return sendHttp("200");
	} else if (cmd=="test_crash") {
		memcpy(0, "t", 1);
	} else if (cmd == "test_leak") {
		//警告: 这里只是测试使用，存在明显的内存泄漏
		//测试一个内存泄漏，以此测试内存泄漏工具是否正常工作
		int *a = new int;
		char *scode = strdup("200");
		return sendHttp(scode);
#endif
	} else if (cmd == "dump") {
#ifdef _WIN32
		coredump(GetCurrentProcessId(),GetCurrentProcess(),NULL);
		//{{ent
#ifndef KANGLE_FREE
		Sleep(2000);
		kthread_pool_start(crash_report_thread, NULL);
#endif//}}
		return sendHttp("200");
#endif
	} else if(cmd=="heapmin") {
#ifdef _WIN32
		_heapmin();
#endif
		return sendHttp("200");
	} else if (cmd == "cache") {
		INT64 csize,cdsize,hsize,hdsize;
		caculateCacheSize(csize,cdsize,hsize,hdsize);
		std::stringstream s;
		s << "<pre>";
		s << "csize:\t" << csize << "\n";
		s << "hsize:\t" << hsize << "\n";
		s << "cdsize:\t" << cdsize << "\n";
		s << "hdsize:\t" << hdsize << "\n";
		s << "</pre>";
		return sendHttp(s.str());
	} else if (cmd == "dump_refs_obj") {
		std::stringstream s;
		s << "<pre>";
		/*
		cacheLock.Lock();
		for (int i=0;i<2;i++) {
			s << "objlist=" << i << "\r\n";
			objList[i].dump_refs_obj(s);
		}
		cacheLock.Unlock();
		*/
		s << "</pre>";
		return sendHttp(s.str());
#ifdef ENABLE_DB_DISK_INDEX
	} else if (cmd=="dci") {
		std::stringstream s;
		if (dci) {
			s << "dci queue: " << dci->getWorker()->queue;
			s << " memory: " << dci->memory_used();
		}
		return sendHttp(s.str());
#endif
#ifdef RQ_LEAK_DEBUG
	} else if (cmd == "dump_connection") {
		selectorManager.dump_all_connection();
		return sendHttp("see server.log");
#endif
	} else {
		return sendHttp("500\ncommand is error");
	}
	return false;
}
bool KHttpManage::extends(unsigned item) {
	stringstream s;
	unsigned i;

	const size_t max_extends = 6;
	const char *extends_header[max_extends] = { klang["single_server"]
#ifdef ENABLE_MULTI_SERVER
			,klang["multi_server"]
#else
			,NULL
#endif

#ifndef HTTP_PROXY
			,klang["api"],
#ifdef ENABLE_CGI
		klang["cgi"],
#else
		NULL,
#endif
		klang["cmd"]
#else
		,NULL,NULL,NULL
#endif
#ifdef ENABLE_KSAPI_FILTER
			,"dso"
#endif
		
	};
	if (item == 0) {
		item = atoi(getUrlValue("item").c_str());
	}
	s << "<html><LINK href=/main.css type='text/css' rel=stylesheet><body>";
	for (i = 0; i < max_extends; i++) {
		if (extends_header[i] == NULL) {
			continue;
		}
		s << "[";
		//if (item != i) {
		s << "<a href=/extends?item=" << i << ">";
		//}
		if (item==i) {
			s << "<font bgcolor=red>";
		}
		s << extends_header[i];
		//if (item != i) {
		s << "</a>";
		//}
		s << "] ";
	}
	s << "<br><br>";
	if (item == 0) {
		s << conf.gam->acserverList(getUrlValue("name"));
	} else if (item == 1) {
#ifdef ENABLE_MULTI_SERVER
		s << conf.gam->macserverList(getUrlValue("name"));
#endif
	} else if (item == 2) {
		string name;
		if (getUrlValue("action") == "edit") {
			name = getUrlValue("name");
		}
		s << conf.gam->apiList(name);
#ifdef ENABLE_CGI
	} else if (item == 3) {
		string name;
		if (getUrlValue("action") == "edit") {
			name = getUrlValue("name");
		}
		s << conf.gam->cgiList(name);
#endif
#ifdef ENABLE_VH_RUN_AS
	} else if (item==4) {
		string name;
		if (getUrlValue("action") == "edit") {
			name = getUrlValue("name");
		}
		s << conf.gam->cmdList(name);
#endif
	} else if (item==5) {
#ifdef ENABLE_KSAPI_FILTER
		if (conf.dem) {
			conf.dem->html(s);
		}
#endif
	}
	s << endTag();
	s << "</body></html>";
	return sendHttp(s.str());
}
bool KHttpManage::sendXML(const char *buf, bool addXml) {
	if (!addXml) {
		return sendHttp(buf, strlen(buf), "text/xml");
	}
	stringstream s;
	s << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" << buf;
	return sendHttp(s.str().c_str(), s.str().size(), "text/xml");
}
bool KHttpManage::config() {
	
	size_t i = 0;

	string config_header[] = { LANG_SERVICE, LANG_CACHE, LANG_LOG,LANG_RS_LIMIT,klang["data_exchange"], LANG_OTHER_CONFIG, LANG_MANAGE_ADMIN};
	size_t max_config = sizeof(config_header)/sizeof(string);
	stringstream s;
	if (xml) {
		KConfigBuilder::build(s);
		return sendXML(s.str().c_str());
	}
	unsigned item = atoi(getUrlValue("item").c_str());
#ifdef KANGLE_ETC_DIR
	string file_name = KANGLE_ETC_DIR;
#else
	string file_name = conf.path + "/etc";
#endif
	bool canWrite = true;
	//	stringstream s;
	conf.admin_lock.Lock();
	file_name += CONFIG_FILE;
	FILE *fp = fopen(file_name.c_str(), "a+");
	if (fp == NULL)
		canWrite = false;
	else
		fclose(fp);
	s << "<html><LINK href=/main.css type='text/css' rel=stylesheet><body>";
	if (!canWrite) {
		s << "<font color=red>" << LANG_CANNOT_WRITE_WARING << file_name
				<< "</font>";
	}
	for (i = 0; i < max_config; i++) {
		//	s << "<td width=12% align=\"center\" bgcolor=\"";
		s << "[";
		if (item == i) {
			s << config_header[i];
		} else {
			s << "<a href=/config?item=" << i << ">" << config_header[i]
					<< "</a>";
		}
		s << "] ";
	}
	s << "<br><hr>";
	s << "<form action=/configsubmit?item=" << item << " method=post>";

	if (item == 0) {
		s << "<table border=0><tr><td valign=top>";		
		s << klang["config_listen"] << ":";
		s << "<table border=1>";
		s << "<tr><td>" << LANG_OPERATOR << "</td><td>" << LANG_IP	<< "</td><td>" << LANG_PORT << "</td><td>" << klang["listen_type"] << "</td></tr>";
		for (size_t i = 0; i < conf.service.size(); i++) {
			s << "<tr><td>";
			s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/deletelisten?id=";
			s << i << "';}\">" << LANG_DELETE << "</a>][<a href='/newlistenform?action=edit&id=" << i << "'>" << LANG_EDIT << "</a>]</td>";
			s << "<td>" << conf.service[i]->ip << "</td>";
			s << "<td>" << conf.service[i]->port << "</td>";
			s << "<td>" << getWorkModelName(conf.service[i]->model) << "</td>";
			s << "</tr>";
		}
		s << "</table>";
		s << "[<a href='/newlistenform'>" << klang["new_listen"] << "</a>]<br>";
		s << klang["connect_time_out"] << ":<input name='connect_time_out' size=5 value='" << conf.connect_time_out << "'><br>";
		s <<  LANG_TIME_OUT << ":<input name='time_out' size=5 value='"	<< conf.time_out << "'><br>";
		s << klang["keep_alive_count"]	<< ":<input name='keep_alive_count' size=6 value='" << conf.keep_alive_count << "'>"  << "<br>";
		s << klang["worker_thread"] << ":<select name='worker_thread'>";
		for (int i=0;i<10;i++) {
			int count = (1<<i)/2;
			s << "<option value='" << count << "' ";
			if (count==conf.select_count) {
				s << "selected";
			}
			s << ">" << count << "</option>";
		}
		s << "</select>";
		s << "</td><td valign=top>";
		s << "\n" << klang["success_listen"] << ":<table border=1>";
		s << "<tr><td>" << LANG_IP	<< "</td><td>" << LANG_PORT;
		s << "</td><td>" << klang["listen_type"] << "</td><td>"	<< klang["protocol"] << "</td><td>flags</td><td>global</td><td>dynamic</td><td>" << LANG_REFS << "</td></tr>";
		conf.gvm->GetListenHtml(s);
		s << "</table>";
		s << "</tr></table>";
	} else if (item == 1) {
		//default
		s << klang["lang_default_cache"] << "<select name=default_cache><option value=1 ";
		if (conf.default_cache > 0) {
			s << "selected";
		}
		s << " > " << LANG_ON << "</option><option value=0 ";
		if (conf.default_cache <=0) {
			s << "selected";
		}
		s << " >" << LANG_OFF << "</option></select><br>";
		s << "" << LANG_TOTAL_MEM_CACHE << ":<input type=text name=mem_cache size=8 value='" << get_size(conf.mem_cache) << "'><br>";
#ifdef ENABLE_DISK_CACHE
		s << LANG_TOTAL_DISK_CACHE << ":<input type=text name=disk_cache size=8 value='" << get_size(conf.disk_cache) << (conf.disk_cache_is_radio? "%": "") << "'><br>";
		s << klang["disk_cache_dir"] << ":<input type=text name=disk_cache_dir value='" << conf.disk_cache_dir2 << "'>[<a href='/format_disk_cache_dir.km'>" << klang["format_disk_cache"] << "</a>]<br>";
		s << klang["disk_work_time"] << ":<input type=text name=disk_work_time value='" << conf.disk_work_time << "'><br>";
#endif
		s << LANG_MAX_CACHE_SIZE << ":<input type=text name=max_cache_size size=6 value='"	<< get_size(conf.max_cache_size) << "'><br>";
#ifdef ENABLE_DISK_CACHE
		s << klang["max_bigobj_size"] << ":<input type=text name=max_bigobj_size size=6 value='" << get_size(conf.max_bigobj_size) << "'><br>";
#ifdef ENABLE_BIG_OBJECT_206
		s << "cache part:<input type=checkbox name='cache_part' value='1' ";
		if (conf.cache_part) {
			s << "checked";
		}
		s << "><br>";
#endif
#endif
		s << LANG_MIN_REFRESH_TIME << ":<input type=text name=refresh_time size=4 value=" << conf.refresh_time << ">" << LANG_SECOND << "<br>";

	} else if (item == 2) {
		s << klang["access_log"] << "<input type=text name='access_log' value=\"" << conf.access_log << "\"></br>\n";
		s << LANG_LOG_ROTATE_TIME << "<input type=text name=log_rotate_time value=\"" << conf.log_rotate << "\"></br>\n";
		s << klang["log_rotate_size"] << ":<input type=text name=log_rotate_size size=6 value=\"" << get_size(conf.log_rotate_size) << "\"></br>\n";
		s << klang["error_rotate_size"] << ":<input type=text name='error_rotate_size' size=6 value=\"" << get_size(conf.error_rotate_size) << "\"></br>\n";
		s << klang["log_level"] << "<input type=text name=log_level value='" << conf.log_level << "'></br>\n";
		s << klang["logs_day"] << "<input type=text name='logs_day' value='" << conf.logs_day << "'></br>\n";
		s << klang["logs_size"] << "<input type=text name='logs_size' value='" << get_size(conf.logs_size) << "'></br>\n";
		s << "log_radio:<input type=text name='log_radio' value='" << conf.log_radio << "'></br>\n";
		s << "<input type=checkbox name='log_handle' value='1' ";
		if (conf.log_handle) {
			s << "checked";
		}
		s << ">" << klang["log_handle"] << "</br>\n";
		s << klang["access_log_handle"] << "<input type=text size='30' name='access_log_handle' value='" << conf.logHandle << "'></br>\n";
		s << klang["log_handle_concurrent"] << "<input type=text name='log_handle_concurrent' value='" << conf.maxLogHandle << "'></br>\n";
	} else if (item == 3) {
		s <<  klang["max_connection"] << ": <input type=text size=5 name=max value=" << conf.max << "><br>";

		s <<  LANG_TOTAL_THREAD_EACH_IP	<< ":<input type=text size=3 title='";
#if 0
		ipLock.Lock();
		KPerIpConnect *per_ip = conf.per_ip_head;
		while (per_ip) {
			char ips2[MAXIPLEN];
			ksocket_ipaddr_ip(&per_ip->src.addr,ips2,sizeof(ips2));
			s << ips2;
			if (per_ip->src.mask_num > 0) {
				s << "/" << (int) per_ip->src.mask_num;
			}
			s << "\t\t";
			if (per_ip->deny) {
				s << "deny";
			} else {
				s << per_ip->max;
			}
			s << "\n";
			per_ip=per_ip->next;
		}
		ipLock.Unlock();
#endif
		s << "' name=max_per_ip value="	<< conf.max_per_ip << "><br>";
		//{{ent
#ifdef ENABLE_BLACK_LIST
		s <<  "<input type=checkbox name=per_ip_deny value=1 ";
		if (conf.per_ip_deny) {
			s << "checked";
		}
		s << ">per_ip_deny<br>";
#endif//}}
		s <<  klang["min_free_thread"]	<< ":<input type=text size=3 name=min_free_thread value='"	<< conf.min_free_thread << "'><br>";

#ifdef ENABLE_REQUEST_QUEUE
		s << klang["max_worker"] << ":<input type=text size=4 name='max_worker' value='";
		s << globalRequestQueue.getMaxWorker() << "'> " << klang["max_queue"] ;
		s << ":<input type=text size=4 name='max_queue' value='";
		s << globalRequestQueue.getMaxQueue() << "'><br>";
#endif
//{{ent
#ifdef ENABLE_FATBOY
		s << klang["bl_time"] << ":<input name='bl_time' size='6' value='" << conf.bl_time << "'><br>";
		s << klang["wl_time"] << ":<input name='wl_time' size='6' value='" << conf.wl_time << "'><br>";
#endif
#ifdef ENABLE_ADPP
		s << klang["process_cpu_usage"]
				<< ":<input type=text size=3 name='process_cpu_usage' value="
				<< conf.process_cpu_usage << "><br>";
#endif
//}}
#ifndef _WIN32
//		s << klang["lang_stack_size"]
//				<< ":<input type=text name='stack_size' value="
//				<< conf.stack_size << "><br>";
#endif
		s << klang["io_worker"] << ":<input type=text size=4 name='worker_io' value='" << conf.worker_io << "'><br>";
		s << "max_io:<input type=text size=4 name='max_io' value='" << conf.max_io << "'><br>";
		s << "io_timeout:<input type=text size=4 name='io_timeout' value='" << conf.io_timeout << "'><br>";
		s << klang["dns_worker"] << ":<input type=text size=4 name='worker_dns' value='" << conf.worker_dns << "'><br>";
		s << "fiber stack size:<input type=text name='fiber_stack_size' size=4 value='" << get_size(conf.fiber_stack_size) << "'><br>";
	} else if (item == 4) {
		//data exchange
#ifdef ENABLE_TF_EXCHANGE	
		s << klang["max_post_size"] << ":<input name='max_post_size' size='4' value='" << get_size(conf.max_post_size) << "'><br>\n";
#endif
		s << "io_buffer:" << get_size(conf.io_buffer) << "<br>";
		//async io
		//s << "<input type=checkbox name='async_io' value='1' " << (conf.async_io ?"checked":"") << ">" << klang["async_io"] << "<br>\n";
		s << "upstream sign : <code title='len=" << conf.upstream_sign_len << "'>" << conf.upstream_sign << "</code><br>\n";
		s << "<!-- read_hup=" << conf.read_hup << " -->\n";
	} else if (item == 5) {
		s  << klang["lang_only_gzip_cache"] << "<select name=only_compress_cache><option value=1 ";
		if (conf.only_compress_cache == 1){
			s << "selected";
		}
		s << " > " << LANG_ON << "</option><option value=0 ";
		if (conf.only_compress_cache != 1){
			s << "selected";
		}
		s << " >" << LANG_OFF << "</option></select><br>";

		s << "min compress length:<input name='min_compress_length' size=6 value='"	<< conf.min_compress_length << "'><br>";
		s << "<a href=# title='(0-9,0=disable)'>gzip level</a><input name=gzip_level size=3 value='"<< conf.gzip_level << "'><br>";
#ifdef ENABLE_BROTLI
		s << "<a href=# title='(0-9,0=disable)'>br level</a><input name=br_level size=3 value='" << conf.br_level << "'><br>";
#endif
		//{{ent
#ifdef KANGLE_ENT
		s << klang["server_software"]
				<< "<input type=text name='server_software' value='"
				<< conf.server_software << "'><br>";
#endif
		//}}
		s << klang["hostname"]	<< "<input type=text name='hostname' value='";
		if(*conf.hostname){
			s << conf.hostname;
		}
		s << "'><br>";
		s << "<input type=checkbox name='path_info' value='1' ";
		if (conf.path_info) {
			s << "checked";
		}
		s << ">" << LANG_USE_PATH_INFO;
		s << "<br>";
#ifdef KSOCKET_UNIX	
		s << "<input type=checkbox name='unix_socket' value='1' ";
		if (conf.unix_socket) {
			s << "checked";
		}
		s << ">" << klang["unix_socket"];
		s << "<br>";
#endif
#ifdef MALLOCDEBUG
		s << "<input type=checkbox name='mallocdebug' value='1' ";
		if (conf.mallocdebug) {
			s << "checked";
		}
		s << ">mallocdebug";
		s << "<br>";
#endif
		//{{ent
#ifdef ENABLE_BLACK_LIST
		s << "<pre>";
		if (*conf.block_ip_cmd) {			
			s << "block cmd:\t" << conf.block_ip_cmd << "\n";
			if (*conf.unblock_ip_cmd) {
				s << "unblock cmd:\t" << conf.unblock_ip_cmd << "\n";
			}
			if (*conf.flush_ip_cmd) {
				s << "flush cmd:\t" << conf.flush_ip_cmd << "\n";
			}			
		}
		if (*conf.report_url) {
			s << "report_url:\t" << conf.report_url << "\n";
		}
		s << "</pre>";
#endif
		//}}
	} else if (item == 6) {
		s << LANG_ADMIN_USER << ":<input name=admin_user value='"
				<< conf.admin_user << "'><br>";
		s << LANG_ADMIN_PASS
				<< ":<input name=admin_passwd autocomplete='off' type=password value=''><br>";
		s << klang["auth_type"];
		for (i = 0; i < TOTAL_AUTH_TYPE; i++) {
			s << "<input type=radio name='auth_type' value='"
					<< KHttpAuth::buildType(i) << "' ";
			if ((unsigned) conf.auth_type == i) {
				s << "checked";
			}
			s << ">" << KHttpAuth::buildType(i) << " ";
		}
		s << "<br>";
		s << "" << LANG_ADMIN_IPS << ":<input name=admin_ips value='";
		for (i = 0; i < conf.admin_ips.size(); i++) {
			s << conf.admin_ips[i] << "|";
		}
		s << "'><br>";
	}
	s << "<br><input type=submit value='" << LANG_SUBMIT << "'></form>"	<< endTag() << "</body></html>";
	conf.admin_lock.Unlock();
	return sendHttp(s.str());
}
bool KHttpManage::configsubmit() {
//	size_t i;

	size_t item = atoi(getUrlValue("item").c_str());
	conf.admin_lock.Lock();
	if (item == 0) {
		conf.set_time_out(atoi(getUrlValue("time_out").c_str()));
		conf.set_connect_time_out(atoi(getUrlValue("connect_time_out").c_str()));
		conf.keep_alive_count = atoi(getUrlValue("keep_alive_count").c_str());
		int worker_thread = atoi(getUrlValue("worker_thread").c_str());
		if (worker_thread!=conf.select_count) {
			conf.select_count = worker_thread;
			need_reboot_flag = true;
		}
		selector_manager_set_timeout(conf.connect_time_out, conf.time_out);
	} else if (item == 1) {
#ifdef ENABLE_DISK_CACHE
		conf.disk_cache = get_radio_size(getUrlValue("disk_cache").c_str(),conf.disk_cache_is_radio);
		string disk_cache_dir = getUrlValue("disk_cache_dir");
		if (disk_cache_dir != conf.disk_cache_dir2) {
			SAFE_STRCPY(conf.disk_cache_dir2,disk_cache_dir.c_str());
			need_reboot_flag = true;
		}
		SAFE_STRCPY(conf.disk_work_time,getUrlValue("disk_work_time").c_str());
		conf.diskWorkTime.set(conf.disk_work_time);
		cache.init();
		conf.max_bigobj_size = get_size(getUrlValue("max_bigobj_size").c_str());
#ifdef ENABLE_BIG_OBJECT_206
		conf.cache_part = getUrlValue("cache_part") == "1";
#endif
#endif
		conf.mem_cache = get_size(getUrlValue("mem_cache").c_str());
		conf.refresh_time = atoi(getUrlValue("refresh_time").c_str());
		conf.max_cache_size = (unsigned) get_size(getUrlValue("max_cache_size").c_str());
		conf.default_cache = atoi(getUrlValue("default_cache").c_str());
	} else if (item == 2) {
		string access_log = getUrlValue("access_log");
		SAFE_STRCPY(conf.log_rotate,getUrlValue("log_rotate_time").c_str());
		conf.log_rotate_size = get_size(getUrlValue("log_rotate_size").c_str());
		conf.error_rotate_size = get_size(getUrlValue("error_rotate_size").c_str());
		conf.log_radio = atoi(getUrlValue("log_radio").c_str());
		conf.log_level = atoi(getUrlValue("log_level").c_str());
		conf.logs_day = atoi(getUrlValue("logs_day").c_str());
		conf.logs_size = get_size(getUrlValue("logs_size").c_str());
		conf.maxLogHandle = atoi(getUrlValue("log_handle_concurrent").c_str());
		SAFE_STRCPY(conf.logHandle ,getUrlValue("access_log_handle").c_str());
		::logHandle.setLogHandle(conf.logHandle);
		conf.log_handle = getUrlValue("log_handle")=="1";
		set_logger();
		if (access_log != conf.access_log) {
			SAFE_STRCPY(conf.access_log ,access_log.c_str());
			accessLogger.place = LOG_FILE;
			std::string logpath;
			if(!isAbsolutePath(conf.access_log)){
				logpath = conf.path;
			}
			logpath+=conf.access_log;
			accessLogger.setPath(logpath);
		}
	} else if (item == 3) {
		size_t max_per_ip = atoi(getUrlValue("max_per_ip").c_str());
		conf.per_ip_deny = atoi(getUrlValue("per_ip_deny").c_str());
		//set_stack_size(getUrlValue("stack_size"));
//{{ent
#ifdef ENABLE_FATBOY
		conf.bl_time = atoi(getUrlValue("bl_time").c_str());
		conf.wl_time = atoi(getUrlValue("wl_time").c_str());
#endif
//}}
		conf.max = atoi(getUrlValue("max").c_str());
		conf.min_free_thread = atoi(getUrlValue("min_free_thread").c_str());
		if (conf.max_per_ip != max_per_ip){
			conf.max_per_ip = max_per_ip;
		}

#ifdef ENABLE_REQUEST_QUEUE
		globalRequestQueue.set(atoi(getUrlValue("max_worker").c_str()),atoi(getUrlValue("max_queue").c_str()));
#endif
//{{ent
#ifdef ENABLE_ADPP
		conf.process_cpu_usage = atoi(getUrlValue("process_cpu_usage").c_str());
#endif
//}}
		conf.worker_io = atoi(getUrlValue("worker_io").c_str());
		conf.worker_dns = atoi(getUrlValue("worker_dns").c_str());
		conf.max_io = atoi(getUrlValue("max_io").c_str());
		conf.io_timeout = atoi(getUrlValue("io_timeout").c_str());
		kasync_worker_set(conf.ioWorker,conf.worker_io,conf.max_io);
		kasync_worker_set(conf.dnsWorker,conf.worker_dns,512);
		int fiber_stack_size = (int)get_size(getUrlValue("fiber_stack_size").c_str());
		fiber_stack_size = kgl_align(fiber_stack_size, 4096);
		if (fiber_stack_size > 0) {
			conf.fiber_stack_size = fiber_stack_size;
		}
	} else if (item == 4) {
		//data exchange
#ifdef ENABLE_TF_EXCHANGE
#if 0
		conf.tmpfile = atoi(getUrlValue("tmpfile").c_str());
		if (conf.tmpfile<0 || conf.tmpfile>2) {
			conf.tmpfile = 1;
		}
#endif
		conf.max_post_size = get_size(getUrlValue("max_post_size").c_str());
#endif
		//conf.async_io = (getUrlValue("async_io")=="1");
	} else if (item == 5) {
#ifdef MALLOCDEBUG
		if(getUrlValue("mallocdebug")=="1"){
			conf.mallocdebug = true;
		} else {
			conf.mallocdebug = false;
		}
#endif
#ifdef KSOCKET_UNIX	
		if (getUrlValue("unix_socket") == "1") {
			conf.unix_socket = true;
		} else {
			conf.unix_socket = false;
		}
#endif
		if (getUrlValue("path_info") == "1") {
			conf.path_info = true;
		} else {
			conf.path_info = false;
		}
		conf.gzip_level = atoi(getUrlValue("gzip_level").c_str());
#ifdef ENABLE_BROTLI
		conf.br_level = atoi(getUrlValue("br_level").c_str());
#endif
		conf.only_compress_cache = atoi(getUrlValue("only_compress_cache").c_str());
		if (conf.gzip_level > 9 || conf.gzip_level < -1){
			conf.gzip_level = -1;
		}
		conf.min_compress_length = atoi(getUrlValue("min_compress_length").c_str());
		conf.setHostname(getUrlValue("hostname").c_str());
		//{{ent
#ifdef KANGLE_ENT
		std::string server_software = getUrlValue("server_software");
		if(server_software!=conf.server_software) {
			SAFE_STRCPY(conf.server_software,server_software.c_str());
			parse_server_software();

		}
#endif
		//}}
	} else if (item == 6) {
		string errMsg;
		if(!changeAdminPassword(&urlValue,errMsg)){
			conf.admin_lock.Unlock();
			return sendErrPage(errMsg.c_str());
		}
	}
	if (!saveConfig()) {
		conf.admin_lock.Unlock();
		return sendErrorSaveConfig();
	}
	conf.admin_lock.Unlock();
	stringstream url;
	url << "/config?item=" << item;
	//	url+=item;
	return sendRedirect(url.str().c_str());
	//return true;
}

KHttpManage::KHttpManage() {
	userType = USER_TYPE_UNAUTH;
	rq = NULL;
	postData = NULL;
	postLen = 0;
	xml = false;
}
KHttpManage::~KHttpManage() {
	if (postData) {
		free(postData);
	}
}
string KHttpManage::getUrlValue(string name) {

	map<string, string>::iterator it;
	it = urlParam.find(name);
	if (it == urlParam.end())
		return "";
	return (*it).second;

}
bool KHttpManage::parseUrlParam(char *param) {
	char *name;
	char *value;
	char *tmp;
	char *msg;
	char *ptr;
	for (size_t i = 0; i < strlen(param); i++) {
		if (param[i] == '\r' || param[i] == '\n') {
			param[i] = 0;
			break;
		}
	}
	//	url_unencode(param);
	//printf("param=%s.\n",param);
	tmp = param;
	char split = '=';
	//	strcpy(split,"=");
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		if (split == '=') {
			name = msg;
			split = '&';
		} else {//strtok_r(msg,"=",&ptr2);
			url_decode(msg, 0, rq->sink->data.url);
			value = msg;//strtok_r(NULL,"=",&ptr2);
			/*
			 if(value==NULL)
			 continue;
			 */
			split = '=';
			for (size_t i = 0; i < strlen(name); i++) {
				name[i] = tolower(name[i]);
			}
			url_decode(name, 0, rq->sink->data.url);
			urlParam.insert(pair<string, string> (name, value));
			urlValue.add(name, value);
		}

	}
	return true;
}

bool KHttpManage::parseUrl(char *url) {

	if (url){
		char *buf = xstrdup(url);
		bool result = parseUrlParam(buf);
		xfree(buf);
		return result;
	}
	return false;

}
bool KHttpManage::sendHttp(const char *msg, INT64 content_length,const char *content_type, const char *add_header, int max_age) {
	KStringBuf s;
	rq->responseStatus(STATUS_OK);

	timeLock.Lock();
	rq->responseHeader(kgl_header_server, conf.serverName, conf.serverNameLength);
	rq->responseHeader(kgl_header_date, (char *)cachedDateTime, 29);
	timeLock.Unlock();
	if (content_type) {
		rq->responseHeader(kgl_expand_string("Content-Type"), content_type, (hlen_t)strlen(content_type));
	} else {
		rq->responseHeader(kgl_expand_string("Content-Type"), kgl_expand_string("text/html; charset=utf-8"));
	}
	if (max_age == 0) {
		rq->responseHeader(kgl_expand_string("Cache-control"), kgl_expand_string("no-cache,no-store"));
	} else {
		s << "public,max_age=" << max_age;
		rq->responseHeader(kgl_expand_string("Cache-control"), s.getBuf(), s.getSize());
	}
	kbuf *gzipOut = NULL;
	rq->responseConnection();
	if (content_length > conf.min_compress_length && msg && KBIT_TEST(rq->sink->data.raw_url.encoding, KGL_ENCODING_GZIP)) {
		kbuf in;
		memset(&in, 0, sizeof(in));
		in.data = (char *)msg;
		in.used = (int)content_length;
		gzipOut = deflate_buff(&in, conf.gzip_level, content_length, true);
		KBIT_SET(rq->sink->data.flags, RQ_TE_COMPRESS);
		rq->responseHeader(kgl_expand_string("Content-Encoding"), kgl_expand_string("gzip"));
	}
	if (content_length>=0) {
		rq->responseHeader(kgl_expand_string("Content-length"), (int)content_length);
    }
	rq->startResponseBody(-1);
    if (gzipOut) {
		bool result = rq->WriteBuff(gzipOut);
        destroy_kbuf(gzipOut);
		return result;
    }
	if (msg == NULL) {
		return true;
	}
	if (content_length <= 0) {
		content_length = strlen(msg);
	}
	return rq->WriteAll(msg, (int)content_length);
}
bool KHttpManage::sendHttp(const string &msg) {
	return sendHttp(msg.c_str(), msg.size());
}
void KHttpManage::sendTest() {
	map<string, string>::iterator it;
	for (it = urlParam.begin(); it != urlParam.end(); it++) {
		printf("name:");
		printf("%s", (*it).first.c_str());
		printf(" value:");
		printf("%s", (*it).second.c_str());
		printf("\n");
	}
}
bool KHttpManage::reboot() {
	stringstream s;
	need_reboot_flag = false;
	s
			<< "<html><head><meta http-equiv=\"Refresh\" content=\"3;url=/\"></head><body>";
	s << "<img border='0' width='0' height='0' src='/reboot_submit?s=" << kgl_current_sec << "&r=" << rand() << "'/>";
	s << "<h2><center>" << klang["rebooting"] << "</center></h2>";
	s << endTag();
	s << "</body></html>";
	bool result = sendHttp(s.str());
	return result;
}
bool KHttpManage::sendErrorSaveConfig(int file) {
	stringstream s;
	s << "Warning: Cann't write file ";
	if (file == 0) {
		s << "etc/config.xml";
	} else if (file == 1) {
		s << VH_CONFIG_FILE;
	}
	s << ",your operator will not save!";
	return sendErrPage(s.str().c_str());
}
bool KHttpManage::sendErrPage(const char *err_msg, int close_flag) {
	stringstream s;
	s
			<< "<html><LINK href=/main.css type='text/css' rel=stylesheet><body><h1><font color=red>"
			<< err_msg;
	if (close_flag == 1) {
		s << "</font></h1><br><a href=\"javascript:window.close();\">close</a>";
	} else {
		s << "</font></h1><br><a href=\"javascript:history.go(-1);\">back</a>";
	}
	s << endTag() << "</body></html>";
	return sendHttp(s.str());
}
bool KHttpManage::sendLeftMenu() {
	stringstream s;
	s << "<html><head><title>" << PROGRAM_NAME << "(" << VER_ID << ") "	<< LANG_MANAGE	<< "</title><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << "<img border=0 src='/logo.gif' alt='logo'>";
	s << "<table><tr><td height=30><a href=/main target=mainFrame>"	<< LANG_HOME << "</a></td></tr>";
	s << "<tr><td height=30><a href='/accesslist?access_type=0' target=mainFrame>"<< klang["lang_requestAccess"] << "</a></td></tr>";
	s << "<tr><td height=30><a href='/accesslist?access_type=1' target=mainFrame>"	<< klang["lang_responseAccess"] << "</a></td></tr>";
	s << "<tr><td height=30><a href=/acserver target=mainFrame>" << klang["extends"] << "</a></td></tr>\n";
	s << "<tr><td height=30><a href='/vhslist' target=mainFrame>" << LANG_VHS	<< "</a></td></tr>\n";
#ifndef HTTP_PROXY
	s << "<tr><td height=30><a href='/process' target=mainFrame>" << klang["process"] << "</a></td></tr>\n";
#endif
#ifdef ENABLE_WRITE_BACK
	//s << "<tr><td height=30><a href=/writeback target=mainFrame>" << LANG_WRITE_BACK << "</a></td></tr>";
#endif
	//if (conf.max_per_ip > 0)
	s << "<tr><td height=30><a href=/connect_per_ip target=mainFrame>" << LANG_CONNECT_PER_IP << "</a></td></tr>";
	s << "<tr><td height=30><a href=/connection target=mainFrame>" << LANG_CONNECTION << "</a></td></tr>";
	s << "<tr><td height=30><a href='/config' target=mainFrame>" << LANG_CONFIG	<< "</a></td></tr>";
	s << "<tr><td height=30><a href=\"javascript:if(confirm('really reboot')){ window.parent.location='/reboot';}\">";
	s << klang["reboot"] << "</a>";
	//*/
	s << "</table></body></html>";
	return sendHttp(s.str().c_str());
}
bool KHttpManage::sendMainFrame() {
	stringstream s;
	char buf[256];
	INT64 total_mem_size = 0, total_disk_size = 0;
	int mem_count = 0, disk_count = 0;
	time_t total_run_time = kgl_current_sec - kgl_program_start_sec;
	cache.getSize(total_mem_size, total_disk_size,mem_count,disk_count);
	s << "<html><head><title>" << PROGRAM_NAME << "(" << VER_ID << ") ";
	s << LANG_INFO << "</title><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << "<table width='98%'><tr><td>";
	s << "<h3>" << klang["program_info"] << "</h3><table border=0>";
	s << "<tr><td>" << klang["version"] << "</td><td>" << VERSION << "(" << getServerType()	<< ") ";
	if(serial > 0){
		s << klang["serial"] << ":" << serial;
	}
	s << "</td></tr>";
	s << "<tr><td>" << klang["path"] << "</td><td>" << conf.path << "</td></tr>";
	s << "</table>";
	s << "<div id='version_note'></div>";
	s << "<h3>" << LANG_OBJ_CACHE_INFO << "</h3>";
	s << "[<a href='#' onClick=\"if(confirm('sure?')){ window.location='/clean_all_cache'}\">" << klang["clean_all_cache"] << "</a>]";
#ifdef ENABLE_DISK_CACHE
	if (index_progress) {
		s << "[scaning...(" << get_index_scan_progress() << "%)]";
	} else if (index_scan_state.need_index_progress) {
		s << "[paused(" << get_index_scan_progress() << "%)]";
	} else {
		s << "[<a href='#' onClick=\"if(confirm('sure?')){ window.location='/scan_disk_cache.km'}\">" << klang["scan_disk_cache"] << "</a>]";
	}
#ifndef NDEBUG
	s << "[<a href='/flush_disk_cache.km'>flush</a>]";
#endif
#endif
	s << "<table><tr><td>";
	s << LANG_TOTAL_OBJ_COUNT << "</td><td colspan=2>" << cache.getCount() << "</td></tr>";
	s << "<tr><td>";
	s << LANG_TOTAL_MEM_CACHE << "</td><td>" << mem_count << "</td><td>" << get_human_size(double(total_mem_size),buf,sizeof(buf)) << "</td></tr>";
#ifdef ENABLE_DISK_CACHE
	s << "<tr><td>";
	s << LANG_TOTAL_DISK_CACHE << "</td><td>" << disk_count << "</td><td>" <<  get_human_size(double(total_disk_size),buf,sizeof(buf)) << "</td></tr>";
	INT64 total_size, free_size;	
	if (get_disk_size(total_size, free_size)) {
		s << "(" << (total_size - free_size) * 100 / total_size << "%)";
	}
#endif
	s << "</table>";
	s << "<h3>" << LANG_UPTIME << "</h3>" << LANG_TOTAL_RUNING << " ";
	if (total_run_time >= 86400) {
		s << total_run_time / 86400 << " " << LANG_DAY << ",";
		total_run_time %= 86400;
	}
	if (total_run_time >= 3600) {
		s << total_run_time / 3600 << " " << LANG_HOUR << ",";
		total_run_time %= 3600;
	}
	if (total_run_time >= 60) {
		s << total_run_time / 60 << " " << LANG_MIN << ",";
		total_run_time %= 60;
	}
	s << total_run_time << " " << LANG_SECOND << ".";
	s << "<h3>" << klang["load_info"] << "</h3>";
	s << "<table>";
	//connect
	s << "<tr><td>" << LANG_CONNECT_COUNT << "</td>";
	s << "<td>" << total_connect << "</td></tr>\n";
	//thread
	int worker_count, free_count;
	kthread_get_count(&worker_count, &free_count);
	s << "<tr><td>fiber</td><td>" << kfiber_get_count() << "</td></tr>";
	s << "<tr><td>" << LANG_WORKING_THREAD << "</td>";
	s << "<td >" << worker_count << "</td></tr>\n";
	s << "<tr><td>" << LANG_FREE_THREAD << "</td><td>" << free_count << "</td></tr>\n";
#ifdef ENABLE_REQUEST_QUEUE
	s << "<tr><td>" << klang["request_worker_info"] << "</td><td>" << globalRequestQueue.getWorkerCount() << "/" << globalRequestQueue.getQueueSize() << "</td></tr>";
	s << "<!-- queue refs=" << globalRequestQueue.getRef() << " -->\n";
#endif
#ifdef ENABLE_STAT_STUB
	s << "<tr><td>async io</td><td>" << katom_get((void *)&kgl_aio_count) << "</td></tr>\n";
#endif
	s << "<tr><td>" << klang["io_worker_info"] << "</td><td>" << conf.ioWorker->worker << "/" << conf.ioWorker->queue << "</td></tr>\n";
	s << "<tr><td>" << klang["dns_worker_info"] << "</td><td>" << conf.dnsWorker->worker << "/" << conf.dnsWorker->queue << "</td></tr>\n";
	s << "<tr><td>addr cache:</td><td>" << kgl_get_addr_cache_count() << "</td></tr>\n";
#ifdef ENABLE_DB_DISK_INDEX
	s << "<tr><td>dci queue:</td><td>" << (dci?dci->getWorker()->queue:0) << "</td></tr>\n";
	s << "<tr><td>dci mem:</td><td>" << (dci ? dci->memory_used():0) << "</td></tr>\n";
#endif
	s << "</table>\n";
//{{ent
#ifdef ENABLE_FATBOY
	//s << "<h3>" << LANG_FATBOY << "</h3>" << LANG_TOTAL_FATBOY << " "	<< net_request_count();
#endif
//}}
	//kselector *selector = selectorManager.newSelector();
	s << "<h3>" << klang["selector"] << "</h3>";
	s << "<table>" ;
	s << "<tr><td>" << LANG_NAME << "</td><td>";
	s << kgl_selector_module.name << "</td></tr>";
	//s << "<tr><td>" << klang["worker_process"] << "</td><td>" << conf.worker << "</td></tr>";
	s << "<tr><td>" << klang["worker_thread"] << "</td><td>";
	s << get_selector_count() << "</td></tr>";
	s << "</table>";

	s << "</td><td valign=top align=right>";
	s
			<< "<form action='/changelang' method='post' target=_top><div align=right>";
	s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_config';}\">" << klang["reload_config"] << "</a>] ";
	//s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_vh';}\">" << klang["reload_vh"] << "</a>] ";
	s << klang["lang"] << ":<select name='lang'>";
	std::vector<std::string> langNames;
	klang.getAllLangName(langNames);
	for (size_t i = 0; i < langNames.size(); i++) {
		s << "<option value='" << langNames[i] << "' ";
		if (strcasecmp(conf.lang, langNames[i].c_str()) == 0) {
			s << "selected";
		}
		s << ">" << langNames[i] << "</option>";
	}
	s << "</select><input type=submit value='" << klang["change_lang"]
			<< "'></div></form>";
	s << "</td></tr></table>";
	//	s << "<h3>Connection Infomation</h3><table border=1><tr><td>src_ip</td><td>service|port</td><td>dst_ip</td><td>dst_port</td><td>connect time</td><td>title</td></tr>";
	s << endTag();
	s << "</body></html>";
	return sendHttp(s.str().c_str());
}
bool KHttpManage::send_css() {
	std::string css = klang["css"];
	return sendHttp(css.c_str(), css.size(), "text/css", NULL, 3600);
}
bool KHttpManage::sendMainPage() {
	stringstream s;
	s << "<html><head><title></title>\n";
	s
			<< "</head><frameset rows=\"*\" cols=\"169,*\" framespacing=\"0\" frameborder=\"NO\" border=\"0\">";
	s << "  <frame src=\"/left_menu\" scrolling=\"NO\" noresize>";
	s
			<< "  <frame src=\"/main\" name=\"mainFrame\"></frameset><noframes><body></body></noframes></html>";
	return sendHttp(s.str());
}
bool KHttpManage::sendRedirect(const char *newUrl) {
	rq->responseStatus(STATUS_FOUND);
	rq->responseHeader(kgl_expand_string("Content-Length"), 0);
	rq->responseConnection();
	rq->responseHeader(kgl_expand_string("Location"), newUrl, (hlen_t)strlen(newUrl));
	return rq->startResponseBody(0);
}
bool matchManageIP(const char *ip, std::vector<std::string> &ips,std::string &matched_ip)
{
	for (size_t i = 0; i < ips.size(); i++) {
		if (ips[i][0]=='~') {
			if (strcasecmp(ips[i].c_str()+1,ip)==0) {
				matched_ip = ips[i];
				return true;
			}
			continue;
		}
		if (ips[i] == "*" || strcasecmp(ips[i].c_str(),ip)==0) {
			matched_ip = ips[i];
			return true;
		}
	}
	return false;
}
char *KHttpManage::parsePostFile(int &len, std::string &fileName) {
	KHttpHeader *header = rq->sink->data.GetHeader();
	char *boundary = NULL;
	if (postData == NULL) {
		return NULL;
	}
	while (header) {
		if (strcasecmp(header->attr, "Content-Type") == 0) {
			boundary = strstr(header->val, "boundary=");
			if (boundary) {
				boundary += 9;
				break;
			}
		}
		header = header->next;
	}
	if (boundary == NULL) {
		return NULL;
	}
	int boundary_len = strlen(boundary);
	char *pstr = strstr(postData, boundary);
	if (pstr == NULL) {
		return NULL;
	}
	pstr = strchr(pstr, '\n');
	if (pstr == NULL)
		return NULL;
	pstr += 1;
	char *next_line;
	char filename[256];
	char *no_dir;
	stringstream file_name;
	for (;;) {
		next_line = strchr(pstr, '\n');
		if (next_line == NULL) {
			return NULL;
		}
		next_line[0] = 0;
		if (pstr[0] == '\r' || pstr[0] == 0)
			break;

		if (strstr(pstr, "name=\"filename\"")) {
			char *org_file = strrchr(pstr, ';');
			if (org_file == NULL) {
				return NULL;
			}

			sscanf(org_file, "%*[^f]filename=\"%[^\"]\"", filename);
			no_dir = strrchr(filename, '\\');
			if (no_dir == NULL) {
				no_dir = strrchr(filename, '/');
			}
			if (no_dir != NULL) {
				no_dir++;
			} else {
				no_dir = filename;
			}
			fileName = filename;
		}
		pstr = next_line + 1;
	}

	pstr = next_line + 1;
	int past_len = pstr - postData;
	int data_len = postLen;
	data_len -= past_len;
	past_len = 0;
	char *may_boundary;
	string boundary_end = "\n--";
	boundary_end += boundary;
	for (;;) {
		if (data_len < boundary_len) {
			goto error;
		}
		may_boundary = (char *) memchr(pstr + past_len, boundary_end[0],
				data_len - past_len);
		if (may_boundary == NULL) {
			goto error;
		}
		past_len = may_boundary - pstr;
		if (strncmp(may_boundary, boundary_end.c_str(), boundary_len + 3) == 0) {//yes it is
			break;
		}
		past_len++;
	}
	if (past_len > 0 && pstr[past_len - 1] == '\r') {
		past_len -= 1;
	}
	len = past_len;
	return pstr;
	error: return NULL;
}
void KHttpManage::parsePostData() {
#define MAX_POST_SIZE	8388608 //8m
	if (rq->sink->data.content_length <= 0 || rq->sink->data.meth != METH_POST) {
		return;
	}
	char *buffer = NULL;
	if (rq->sink->data.content_length > MAX_POST_SIZE) {
		fprintf(stderr, "post size is too big\n");
		return;
	}
	buffer = (char *)xmalloc((int)(rq->sink->data.content_length+1));
	char *str = buffer;		
	int length = 0;
	while (rq->sink->data.left_read > 0) {
		length = rq->Read(str, (int)rq->sink->data.left_read);
		if (length <= 0) {
			free(buffer);
			KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
			return;
		}		
		str += length;
	}
	postLen = (int)rq->sink->data.content_length;
	buffer[postLen] = 0;	
	postData = buffer;
}
bool checkManageLogin(KHttpRequest *rq) {

	const char *x_real_ip = rq->GetHttpValue(X_REAL_IP_HEADER);
	if (x_real_ip != NULL) {
		rq->sink->Shutdown();
		return false;
	}
#ifdef KSOCKET_UNIX
	kserver *server = rq->sink->GetBindServer();
	if (server && server->addr.v4.sin_family == AF_UNIX) {
		return true;
	}
#endif
	char ips[MAXIPLEN];
	rq->sink->get_peer_ip(ips, sizeof(ips));
	if (strcmp(ips, "127.0.0.1") != 0) {
		std::string manage_sec_file = conf.path  + "manage.sec";
		KFile file;
		if (file.open(manage_sec_file.c_str(), fileRead)) {
			rq->sink->Shutdown();
			return false;
		}
	}
	if (!conf.admin_ips.empty()) {		
		std::string matched_ip;
		if (!matchManageIP(ips, conf.admin_ips,matched_ip)) {
			return false;
		}
		if (matched_ip[0]=='~') {
			return true;
		}
	}
	if (rq->auth==NULL) {
		return false;
	}
	if (rq->auth->getType() != conf.auth_type) {
		return false;
	}
	if (conf.admin_user == rq->auth->getUser()
		&& rq->auth->verify(rq, conf.admin_passwd.c_str(), conf.passwd_crypt)) {			
		return true;
	}
	return false;	
}
bool KHttpManage::start_listen(bool &hit) {
	string err_msg;
	if (strcmp(rq->sink->data.url->path, "/deletelisten") == 0) {
		hit = true;
		int id = atoi(getUrlValue("id").c_str());
		conf.admin_lock.Lock();
		if (id >= 0 && id < (int)conf.service.size()) {
			vector<KListenHost *>::iterator it = conf.service.begin() + id;
			conf.gvm->UnBindGlobalListen((*it));
			delete (*it);
			conf.service.erase(it);
			if (!saveConfig()) {
				conf.admin_lock.Unlock();
				return sendErrorSaveConfig();
			}
		}
		conf.admin_lock.Unlock();
		//conf.gvm->flush_static_listens(conf.service);
		return sendRedirect("/config");
	}
	if (strcmp(rq->sink->data.url->path, "/newlisten") == 0) {
		hit = true;
		string ip = getUrlValue("ip");
		if (ip.size() == 0) {
			ip = "*";
		}
		int model;
		if (!parseWorkModel(getUrlValue("type").c_str(), model)) {
			return sendErrPage("type is error");
		}
		KListenHost * host = new KListenHost;;
		string action = getUrlValue("action");
		conf.admin_lock.Lock();
		/*
		
		*/
		//need_reboot_flag = true;
#ifdef KSOCKET_SSL
		if (KBIT_TEST(model,WORK_MODEL_SSL)) {
			host->cert_file = getUrlValue("certificate");
			host->key_file = getUrlValue("certificate_key");
			host->cipher = getUrlValue("cipher");
			host->protocols = getUrlValue("protocols");
			host->early_data = getUrlValue("early_data") == "1";
#ifdef ENABLE_HTTP2
			host->http2 = getUrlValue("http2")=="1";
#endif
		}
#endif
		host->ip = ip;
		host->port = getUrlValue("port");
		host->model = model;
		conf.gvm->BindGlobalListen(host);
		if (action == "edit") {
			int id = atoi(getUrlValue("id").c_str());
			if (id >= 0 && id < (int)conf.service.size()) {
				conf.gvm->UnBindGlobalListen(conf.service[id]);
				delete conf.service[id];
				conf.service[id] = host;
			} else {
				conf.service.push_back(host);
			}
		} else {
			conf.service.push_back(host);
		}		
		conf.admin_lock.Unlock();
		if (!saveConfig()) {
			return sendErrorSaveConfig();
		}
		return sendRedirect("/config");

	}
	if (strcmp(rq->sink->data.url->path, "/newlistenform") == 0) {
		hit = true;
		stringstream s;
		int id = atoi(getUrlValue("id").c_str());
		KListenHost * host = NULL;
		if (getUrlValue("action") == "edit") {
			if (id < 0 || id >= (int)conf.service.size()) {
				return sendErrPage("cann't find such listen");
			}
			host = conf.service[id];
		}
		s
				<< "<html><head><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
#ifdef KSOCKET_SSL
        s << "<script language='javascript'>"
                "function $(id) \
                { \
                if (document.getElementById) \
                        return document.getElementById(id); else if (document.all)\
                        return document.all(id);return document.layers[id];}\
                function show_div(div_name,flag){var el=$(div_name);if(flag)\
                el.style.display='';\
                else\
                el.style.display='none';}"
                "function changeModel() {"
                " if(listen.type.value=='https' || listen.type.value=='manages' || listen.type.value=='tcps' ){"
					"show_div('ssl',true);"
                "} else {"
					"show_div('ssl',false);"
                "}"
                "}"
                "</script>";
#endif
		s << "<form name='listen' action='/newlisten?action=" << getUrlValue("action")
				<< "&id=" << id << "' method='post'>\n";
		s << "<table>";
		/*
		s << "<tr><td>" << LANG_NAME << ":</td><td><input name='name' value='"
				<< (host ? host->name : "") << "'></td></tr>";
		*/
		s << "<tr><td>" << LANG_IP << ":</td><td><input name='ip' value='"
				<< (host ? host->ip : "*") << "'></td></tr>";
		s << "<tr><td>" << LANG_PORT << ":</td><td><input name='port' value='"
				<< (host ? host->port : "80") << "'>";
		s << "</td></tr>";
		s << "<tr><td>" << klang["listen_type"] << ":</td><td>";
		static const char *model[] = { "http", "manage"
#ifdef KSOCKET_SSL
				, "https", "manages"
#endif
#ifdef WORK_MODEL_TCP
				,"tcp"
#ifdef HTTP_PROXY
				,"tcps"
#endif
#endif		
		};
		s << "<select name='type'  onChange='changeModel()'>";
		for (size_t i = 0; i < sizeof(model) / sizeof(char *); i++) {
			s << "<option value='" << model[i] << "' ";
			if (host && strcasecmp(model[i], getWorkModelName(host->model))
					== 0) {
				s << "selected";
			}
			s << ">" << model[i] << "</option>\n";
		}
		s << "</select>";
		s << "</td></tr>";
#ifdef KSOCKET_SSL
        s << "<tr><td colspan=2>";
        s << "<div id='ssl' style=\"display:";
        if(host==NULL || !KBIT_TEST(host->model,WORK_MODEL_SSL)){
                s << "none";
        }
        s << "\">";
        s << klang["ssl_config"] << "<br>("
                        << klang["ssl_config_note"] << ")<br>";
        s << klang["cert_file"] << "<input name=certificate size=32 value='"
                        << (host ? host->cert_file : "") << "'><br>";
        s << klang["private_file"]
                        << "<input name='certificate_key' size=32 value='"
                        << (host ? host->key_file : "") << "'><br>";
		s << "cipher:<input name='cipher' size=32 value='" << (host ? host->cipher : "") << "'><br>";
		s << "protocols:<input name='protocols' size=32 value='" << (host ? host->protocols : "") << "'><br>";
#if (ENABLE_HTTP2 && TLSEXT_TYPE_next_proto_neg)
		s << "<input type='checkbox' name='http2' value='1' ";
		if(host==NULL || host->http2){
			s << "checked";
		}
		s << ">http2";
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
		s << "<input type='checkbox' name='early_data' value='1' ";
		if (host && host->early_data) {
			s << "checked";
		}
		s << ">early_data";
#endif
        s << "</div></td></tr>";
#endif

		s << "<tr><td colspan=2><input type=submit value='" << LANG_SUBMIT
				<< "'></td></tr>";
		s << "</table>";
		s << "</body></html>";
		return sendHttp(s.str());
	}

	return false;
}
bool KHttpManage::save_access(KVirtualHost *vh,std::string redirect_url)
{
	if (vh) {
#ifndef HTTP_PROXY
		vh->saveAccess();
#endif
		vh->destroy();
	} else {
		if(!saveConfig()){
			sendErrorSaveConfig();
			return false;
		}
	}
	return sendRedirect(redirect_url.c_str());
}
bool KHttpManage::start_access(bool &hit)
{
	hit = true;
	int type = KAccess::getType(atoi(getUrlValue("access_type").c_str()));
	stringstream accesslist;
	std::string name = getUrlValue("vh");
	KVirtualHost *vh = NULL;
	KAccess *access = &kaccess[type];
#ifndef HTTP_PROXY
	if(name.size()>0){
		vh = conf.gvm->refsVirtualHostByName(name);
		if(vh==NULL){
			return sendHttp("cann't find such vh");
		}
		if(vh->user_access.size()==0){
			vh->destroy();
			return sendHttp("vh do not support user access");
		}
		access = &vh->access[type];
	}		
#endif
	accesslist << "/accesslist?access_type=" << getUrlValue("access_type") << "&vh=" << getUrlValue("vh");
	if (strcmp(rq->sink->data.url->path, "/accesslist") == 0) {
		std::stringstream s;
		if(vh){
			vh->destroy();
			conf.gvm->getHtml(s,name,type+6,urlValue);
		} else {
			s << kaccess[type].htmlAccess();
		}
		return sendHttp(s.str());
	}
	
	if (strcmp(rq->sink->data.url->path, "/addmodel") == 0) {
		bool mark = false;
		//		int table = REQUEST;
		//		std::string model = getUrlValue("model");
		if (getUrlValue("mark") == "1") {
			mark = true;
		}
		access->addAcl(getUrlValue("table_name"), atoi(
				getUrlValue("id").c_str()), getUrlValue("model"), mark);
		stringstream url;
		url << "/editchainform?access_type=" << getUrlValue("access_type")
				<< "&table_name=" << getUrlValue("table_name") << "&id="
				<< getUrlValue("id") << "&vh=" << getUrlValue("vh");
		return save_access(vh,url.str());
	}
	if (strcmp(rq->sink->data.url->path,"/downmodel")==0) {
		bool mark = false;
		if (getUrlValue("mark") == "1") {
			mark = true;
		}
		access->downModel(getUrlValue("table_name"), atoi(getUrlValue("id").c_str()), getUrlValue("model"), mark);
		stringstream url;
		url << "/editchainform?access_type=" << getUrlValue("access_type")
				<< "&table_name=" << getUrlValue("table_name") << "&id="
				<< getUrlValue("id") << "&vh=" << getUrlValue("vh");
		return save_access(vh,url.str());
	}
	if (strcmp(rq->sink->data.url->path, "/delmodel") == 0) {
		bool mark = false;
		if (getUrlValue("mark") == "1") {
			mark = true;
		}
		access->delAcl(getUrlValue("table_name"), atoi(
				getUrlValue("id").c_str()), getUrlValue("model"), mark);
		stringstream url;
		url << "/editchainform?access_type=" << getUrlValue("access_type")
				<< "&table_name=" << getUrlValue("table_name") << "&id="
				<< getUrlValue("id") << "&vh=" << getUrlValue("vh");
		return save_access(vh,url.str());
	}
	if (strcmp(rq->sink->data.url->path, "/downchain") == 0) {
		access->downChain(getUrlValue("table_name"), atoi(getUrlValue("id").c_str()));
		return save_access(vh,accesslist.str());
	}
	if (strcmp(rq->sink->data.url->path, "/addchain") == 0) {
		int id = access->newChain(getUrlValue("table_name"), atoi(
				getUrlValue("id").c_str()));
		stringstream url;
		url << "/editchainform?access_type=" << getUrlValue("access_type")
				<< "&table_name=" << getUrlValue("table_name") << "&id=" << id << "&vh=" << getUrlValue("vh");
		return save_access(vh,url.str());
	}
	if (strcmp(rq->sink->data.url->path, "/editchain") == 0) {
		stringstream url;
		if (getUrlValue("modelflag") == "1") {
			bool mark = false;
			if (getUrlValue("mark") == "1") {
				mark = true;
			}
			access->addAcl(getUrlValue("table_name"), atoi(getUrlValue(
					"id").c_str()), getUrlValue("modelname"), mark);
			url << "/editchainform?access_type=" << getUrlValue("access_type")
					<< "&table_name=" << getUrlValue("table_name") << "&id="
					<< getUrlValue("id") << "&vh=" << getUrlValue("vh");
		}
		access->editChain(getUrlValue("table_name"), atoi(getUrlValue(
				"id").c_str()), &urlValue);
		if (getUrlValue("modelflag") == "1") {
			return save_access(vh,url.str());
		}
		return save_access(vh,accesslist.str());
	}
	if (strcmp(rq->sink->data.url->path, "/editchainform") == 0) {
		//sendHeader(200);
		std::stringstream s;
		std::stringstream url;
		if (vh) {
			conf.gvm->getMenuHtml(s,vh,url,0);
		}
		s << access->addChainForm(name.c_str(),getUrlValue("table_name"),atoi(getUrlValue("id").c_str()),(getUrlValue("add") == "1" ? true : false));
		s << endTag();
		//int type = KAccess::getType(atoi(getUrlValue("access_type").c_str()));
		bool result = sendHttp(s.str());
		if(vh){
			vh->destroy();
		}
		return result;
	}
	if (strcmp(rq->sink->data.url->path, "/delchain") == 0) {
		if (!access->delChain(getUrlValue("table_name"), atoi(
				getUrlValue("id").c_str()))) {
			if(vh){
				vh->destroy();
			}
			return sendErrPage("Del access failed");
		}
		return save_access(vh,accesslist.str());
	}
	if (strcmp(rq->sink->data.url->path, "/accesschangefirst") == 0) {
		string name;
		int jump_type = atoi(getUrlValue("jump_type").c_str());
		switch (jump_type) {
		case JUMP_SERVER:
			name = getUrlValue("server");
			break;
		case JUMP_WBACK:
			name = getUrlValue("wback");
			break;
		case JUMP_VHS:
			name = getUrlValue("vhs");
			break;
		}
		access->changeFirst(jump_type, name);
		return save_access(vh,accesslist.str());

	}
	/*	if(strcmp(rq->sink->data.url->path,"/tableaddform")==0) {
	 stringstream s;
	 s << "<html><head><title></title><LINK href=/main.css type='text/main.css' rel=stylesheet></head><body><form action=tableadd method=get>\n";
	 s << LANG_TABLE << LANG_NAME << "<input name=table_name><input type=submit value=" << LANG_SUBMIT << "></form></body></html>";
	 return sendHttp(s.str());

	 }*/
	if (strcmp(rq->sink->data.url->path, "/tableadd") == 0) {
		//	stringstream s;
		string err_msg;

		if (access->newTable(getUrlValue("table_name"), err_msg)) {
			//conf.m_kfilter.SaveConfig();
			return save_access(vh,accesslist.str());
		} else {
			if(vh){
				vh->destroy();
			}
			return sendErrPage(err_msg.c_str());
		}
		//	return sendHttp(s.str());
	}
	if (strcmp(rq->sink->data.url->path, "/tableempty") == 0) {
		string err_msg;
		if (!access->emptyTable(getUrlValue("table_name"), err_msg)) {
			if (vh) {
				vh->destroy();
			}
			return sendErrPage(err_msg.c_str());
		}
		return save_access(vh,accesslist.str());
	}
	if (strcmp(rq->sink->data.url->path, "/tabledel") == 0) {
		string err_msg;
		if (!access->delTable(getUrlValue("table_name"), err_msg)) {
			if (vh) {
				vh->destroy();
			}
			return sendErrPage(err_msg.c_str());
		}
		return save_access(vh,accesslist.str());
	}
	if (strcmp(rq->sink->data.url->path, "/tablerename") == 0) {
		string err_msg;
		if (!access->renameTable(getUrlValue("name_from"), getUrlValue(
				"name_to"), err_msg)) {
			if (vh) {
				vh->destroy();
			}
			return sendErrPage(err_msg.c_str());
		}
		return save_access(vh,accesslist.str());
	}
	if (vh) {
		vh->destroy();
	}
	hit = false;
	return false;
}
bool KHttpManage::start_vhs(bool &hit) {
	string err_msg;
	stringstream s;
	if (xml && strcmp(rq->sink->data.url->path, "/vhs") == 0) {

		hit = true;
		conf.gvm->build(s);
		return sendXML(s.str().c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/vhslist") == 0) {
		hit = true;
		conf.gvm->getHtml(s, "", 0,urlValue);
		return sendHttp(s.str());
		//return sendHttp(conf.gvm->getVhsList());
	}
	if (strcmp(rq->sink->data.url->path, "/vhlist") == 0) {
		hit = true;
		conf.gvm->getHtml(s, getUrlValue("name"), atoi(getUrlValue(
				"id").c_str()),urlValue);
		return sendHttp(s.str());
	}
	if(strcmp(rq->sink->data.url->path,"/reload_vh")==0){
		do_config(false);
		hit = true;
		s << klang["reload_vh_msg"] << "<br>";
		conf.gvm->getHtml(s, getUrlValue("name"), atoi(getUrlValue(
				"id").c_str()),urlValue);
		return sendHttp(s.str());
	}
	if (strcmp(rq->sink->data.url->path, "/vhbase") == 0) {
		hit = true;
		conf.gvm->vhBaseAction(urlValue, err_msg);
		if (err_msg.size() > 0) {
			return sendErrPage(err_msg.c_str());
		}
		if (!conf.gvm->saveConfig(err_msg)) {
			return sendErrPage(err_msg.c_str());//sendErrorSaveConfig(1);
		}
		stringstream s;
		s << "/vhlist?id=" << getUrlValue("id") << "&t=" << getUrlValue("t");
		if (getUrlValue("action") != "vh_edit" && getUrlValue("action") != "vh_add" ) {
			s << "&name=" << getUrlValue("name");
		}
		return sendRedirect(s.str().c_str());
	}
	return true;
}
bool KHttpManage::sendProcessInfo() {

	stringstream s;
	s << "<html><head><title>" << PROGRAM_NAME << "(" << VER_ID << ") "
			<< LANG_MANAGE
			<< "</title><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << klang["process_info"] << "<br>";
	spProcessManage.getProcessInfo(s);
#ifdef ENABLE_VH_RUN_AS
	conf.gam->getProcessInfo(s);
#endif
	s << endTag();
	return sendHttp(s.str());
}
KGL_RESULT KHttpManage::Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out)
{
	this->rq = rq;
	bool hit = true;
	if (!start(hit)) {
		KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
	}
	if (!hit) {
		return send_error2(rq, STATUS_NOT_FOUND, "no such file");
	}
	return out->f->write_end(out,rq,KGL_OK);
}
bool KHttpManage::start(bool &hit) {

	parseUrl(rq->sink->data.url->param);
	if (getUrlValue("xml") == "1") {
		xml = true;
	}
	parsePostData();
	if (strcmp(rq->sink->data.url->path, "/command") == 0) {
		return runCommand();
	}
	if (postData) {
		parseUrlParam(postData);
	}
	if (strcmp(rq->sink->data.url->path, "/test") == 0) {
		sendTest();
		return false;
	}
	//	continue_check:
	if (strcmp(rq->sink->data.url->path, "/left_menu") == 0) {
		return sendLeftMenu();
	}
	if (strcmp(rq->sink->data.url->path, "/main") == 0) {
		return sendMainFrame();
	}

	if (strcmp(rq->sink->data.url->path, "/config") == 0) {
		return config();

	}
	if (strcmp(rq->sink->data.url->path, "/configsubmit") == 0) {
		return configsubmit();
	}
	if (strcmp(rq->sink->data.url->path, "/acserver") == 0) {
		return extends();
		//	return sendHttp(
		//			conf.gam->acserverList(getUrlValue("name")).c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/apilist") == 0) {
		return extends(2);
	}
	if (strcmp(rq->sink->data.url->path, "/cgilist") == 0) {
		return extends(3);
	}
	if (strcmp(rq->sink->data.url->path, "/macserver") == 0) {
		return extends(1);
		//return sendHttp(conf.gam->macserverList());
	}
	if (strcmp(rq->sink->data.url->path, "/extends") == 0) {
		return extends();
	}
#ifdef ENABLE_WRITE_BACK
	if (strcmp(rq->sink->data.url->path, "/writeback") == 0) {
		return sendHttp(
				writeBackManager.writebackList(getUrlValue("name")).c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/writebackadd") == 0) {
		string err_msg;
		string msg = getUrlValue("msg");
		if (writeBackManager.newWriteBack(getUrlValue("name"), getUrlValue(
				"msg"), err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/writeback");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/writebackedit") == 0) {
		string err_msg;
		KWriteBack m_a;
		m_a.name = getUrlValue("name");
		m_a.setMsg(getUrlValue("msg"));
		//	m_a.ip=inet_addr(getUrlValue("ip").c_str());
		//	m_a.port=atoi(getUrlValue("port").c_str());
		if (writeBackManager.editWriteBack(getUrlValue("namefrom"), m_a,
				err_msg)) {
			//conf.m_kfilter.SaveConfig();
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/writeback");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcasecmp(rq->sink->data.url->path, "/writebackdelete") == 0) {
		string err_msg;
		if (writeBackManager.delWriteBack(getUrlValue("name"), err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/writeback");
		}
		return sendErrPage(err_msg.c_str());
	}
#endif
#ifdef ENABLE_MULTI_SERVER
	if (strcmp(rq->sink->data.url->path, "/macserveradd") == 0) {
		string err_msg;
		bool edit = getUrlValue("action")=="edit";
		string name = getUrlValue("name");
		if (edit) {
			KMultiAcserver *as = conf.gam->refsMultiAcserver(name);
			if(as==NULL){
				return sendErrPage("cann't find server");
			}
			as->parse(urlValue.attribute);
			as->release();
			saveConfig();
			return sendRedirect("/macserver");
		}
		KPoolableRedirect *rd = conf.gam->refsAcserver(name);
		if(rd){
			rd->release();
			return sendErrPage("server name already used");
		}
		KMultiAcserver *as = new KMultiAcserver;

		as->name = name;
		as->parse(urlValue.attribute);
		if (!conf.gam->addMultiAcserver(as)) {
			as->release();
		}
		saveConfig();
		return sendRedirect("/macserver");
	}
	if (strcmp(rq->sink->data.url->path, "/del_macserver") == 0) {
		string err_msg;
		if (conf.gam->delMAcserver(getUrlValue("name"), err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/macserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/macserver_node_form") == 0) {
		return sendHttp(conf.gam->macserver_node_form(
				getUrlValue("name"), getUrlValue("action"), atoi(getUrlValue(
						"id").c_str())));
	}
	if (strcmp(rq->sink->data.url->path, "/macserver_node") == 0) {
		string err_msg;
		if (conf.gam->macserver_node(urlParam, err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/macserver");
		}
		return sendErrPage(err_msg.c_str());
	}
#endif
	if (strcmp(rq->sink->data.url->path, "/acserveradd") == 0) {
		string err_msg;
		if (conf.gam->newSingleAcserver(false,urlValue.attribute , err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/acserveredit") == 0) {
		string err_msg;
		if (conf.gam->newSingleAcserver(true, urlValue.attribute, err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/acserverdelete") == 0) {
		string err_msg;
		if (conf.gam->delAcserver(getUrlValue("name"), err_msg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path,"/reload_config")==0) {
		do_config(false);
		wait_load_config_done();
		return sendMainFrame();
	}
	if (strcmp(rq->sink->data.url->path, "/connect_per_ip") == 0) {
		return sendHttp(get_connect_per_ip().c_str());
	}

	if (strcasecmp(rq->sink->data.url->path, "/connection") == 0) {
		std::stringstream s;
		KConnectionInfoContext ctx;
		ctx.total_count = 0;
		ctx.debug = atoi(getUrlValue("debug").c_str());
		ctx.vh = NULL;
		ctx.translate = true;

		selector_manager_iterator((void *)&ctx, kconnection_info_iterator);
		
		s << "<html><head><LINK href=/main.css type='text/css' rel=stylesheet></head><body>\n";
		s << "<!-- total_connect=" << total_connect << " -->\n<h3>";
		s << LANG_CONNECTION << "(total:" << ctx.total_count;
		s << ")</h3>\n";
		//[<a href=\"javascript:if(confirm('really?')){ window.location='/close_all_request';}\">" << klang["close_all_request"];
		//s << "</a>]
		s << "<!-- current_msec=" << kgl_current_msec << "-->\n";
		s << "<div id='rq'>loading...</div>\n";
		s << endTag();
		s << "<script language='javascript'>\nvar sortIndex = 2;\nvar sortDesc = false;\nvar rqs=new Array();\n";
		s << ctx.s.str() ;
		s << "function $(id) \n"
		"{ \n"
		"if (document.getElementById) \n"
		"	return document.getElementById(id); "
		"else if (document.all)\n"
		"	return document.all(id);"
		"return document.layers[id];"
		"}\n"
		"function show_url(url,len) {\n"
			"var s='<a href=\\''+url+'\\' title=\\'' + url + '\\' target=_blank>';\n"
			"if(url.length>len) { s += url.substr(0,len) + '...';} else { s += url;} \n"
			"s += '</a>';"
			"return s;\n"
		"}\n"
		"function showRequest(){\
		 var s='<table border=1><tr><td><a href=\\'javascript:sortrq(1)\\'>"
		 << LANG_SRC_IP << "</a></td><td><a href=\\'javascript:sortrq(2)\\'>";		
		s << LANG_CONNECT_TIME;
		s << "</a></td><td><a href=\\'javascript:sortrq(3)\\'>" << LANG_STATE << "</a>"
				<< "</td><td><a href=\\'javascript:sortrq(4)\\'>method</a></td>";
		s << "<td><a href=\\'javascript:sortrq(5)\\'>" << LANG_URL << "</a></td><td><a href=\\'javascript:sortrq(7)\\'>referer</a></td>\
			<td><a href=\\'javascript:sortrq(8)\\'>HTTP</a></td></tr>';\
		for(var i=0;i<rqs.length;i++){\n"
			"s +='<tr>';\n"
			"for(var j=1;j<6;j++){\n"
				" if (j==1) {"
					" s += '<td><div title=\"server ip ' + rqs[i][6] + '\">' + rqs[i][j] + '</div></td>';"
				" } else if (j==5) { \n"
					"s += '<td>'+show_url(rqs[i][j],50)+'</td>';\n"
				"} else { "
					"s += '<td>'+rqs[i][j]+'</td>';\n"
				"}\n"
			"}\n"
			"s += '<td>' + show_url(rqs[i][7],50) + '</td>';"	
			"s += '<td>' + rqs[i][8] + '</td>';"
			"s += '</tr>';\n\
		}\n\
		s += '</table>';\
		$('rq').innerHTML=s;\
	}\n\
function sortRequest(a,b)\
{\
	if (sortIndex==2) {\
		if (sortDesc) {\
			return b[sortIndex] - a[sortIndex];\
		} else {\
			return a[sortIndex] - b[sortIndex];\
		}\
	}\
	if(sortDesc){\
		return b[sortIndex].localeCompare(a[sortIndex]);\
	}		\
	return a[sortIndex].localeCompare(b[sortIndex]);\
}\
function sortrq(index)\
{\
	if(sortIndex!=index){\
		sortDesc = false;\
		sortIndex = index;\
	} else {\
		sortDesc = !sortDesc;\
	}\
	rqs.sort(sortRequest);showRequest();\
}";
		s << "sortrq(2);</script>";
		s << "</body></html>";
		return sendHttp(s.str().c_str());
	}

#ifdef ENABLE_CGI
	if (strcmp(rq->sink->data.url->path, "/cgienable") == 0) {
		bool enable = false;
		if (getUrlValue("flag") == "enable") {
			enable = true;
		}
		if (conf.gam->cgiEnable(getUrlValue("name"), enable)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(3);
		}
		return sendErrPage("error set flag");
	}
#endif
	if (strcmp(rq->sink->data.url->path, "/apienable") == 0) {
		bool enable = false;
		if (getUrlValue("flag") == "enable") {
			enable = true;
		}
		if (conf.gam->apiEnable(getUrlValue("name"), enable)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(2);
		}
		return sendErrPage("error set flag");
	}
#ifdef ENABLE_VH_RUN_AS
	if (strcmp(rq->sink->data.url->path, "/cmdenable") == 0) {
		bool enable = false;
		if (getUrlValue("flag") == "enable") {
			enable = true;
		}
		if (conf.gam->cmdEnable(getUrlValue("name"), enable)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(4);
		}
		return sendErrPage("error set flag");
	}
	if (strcmp(rq->sink->data.url->path, "/cmdform")==0) {
		std::string errMsg;
		if (conf.gam->cmdForm(urlParam, errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/extends?item=4");
		}
		return sendErrPage(errMsg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/delcmd") == 0) {
		std::string errMsg;
		if (conf.gam->delCmd(getUrlValue("name"), errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(4);
		}
		return sendErrPage(errMsg.c_str());

	}
#endif
	if (strcmp(rq->sink->data.url->path, "/apiform") == 0) {
		std::string errMsg;
		if (conf.gam->apiForm(urlParam, errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/extends?item=2");
		}
		return sendErrPage(errMsg.c_str());
	}
#ifdef ENABLE_CGI
	if (strcmp(rq->sink->data.url->path, "/cgiform") == 0) {
		std::string errMsg;
		if (conf.gam->cgiForm(urlParam, errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return sendRedirect("/extends?item=3");
		}
		return sendErrPage(errMsg.c_str());
	}
#endif
	if (strcmp(rq->sink->data.url->path, "/delapi") == 0) {
		std::string errMsg;
		if (conf.gam->delApi(getUrlValue("name"), errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(2);
		}
		return sendErrPage(errMsg.c_str());

	}
#ifdef ENABLE_CGI
	if (strcmp(rq->sink->data.url->path, "/delcgi") == 0) {
		std::string errMsg;
		if (conf.gam->delCgi(getUrlValue("name"), errMsg)) {
			if (!saveConfig()) {
				return sendErrorSaveConfig();
			}
			return extends(3);
		}
		return sendErrPage(errMsg.c_str());

	}
#endif
	if (strcmp(rq->sink->data.url->path, "/changelang") == 0) {
		strncpy(conf.lang, getUrlValue("lang").c_str(),sizeof(conf.lang)-1);
		if (!saveConfig()) {
			return sendErrorSaveConfig();
		}
		return sendRedirect("/");
	}
	if (strcmp(rq->sink->data.url->path, "/process") == 0) {
		return sendProcessInfo();
	}
	if (strcmp(rq->sink->data.url->path, "/process_kill") == 0) {
		killProcess(getUrlValue("name"),getUrlValue("user"),atoi(getUrlValue("pid").c_str()));
		return sendRedirect("/process");
	}
	if (strcmp(rq->sink->data.url->path, "/reboot") == 0) {
		return reboot();
	}
	if (strcmp(rq->sink->data.url->path, "/reboot_submit")==0) {
		KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
		console_call_reboot();
		return sendHttp("");
	}
	if(strcmp(rq->sink->data.url->path,"/clean_all_cache")==0){
		dead_all_obj();
		return sendMainFrame();
	}
#ifdef ENABLE_DISK_CACHE
	if (strcmp(rq->sink->data.url->path,"/scan_disk_cache.km")==0) {
		rescan_disk_cache();
		return sendMainFrame();
	}
	if (strcmp(rq->sink->data.url->path,"/flush_disk_cache.km")==0) {
		cache.flush_mem_to_disk();
		saveCacheIndex();
		return sendMainFrame();
	}
	void create_cache_dir(const char *disk_dir);
	if (strcmp(rq->sink->data.url->path,"/format_disk_cache_dir.km")==0) {
		string dir = getUrlValue("dir");
		if (!dir.empty()) {
			SAFE_STRCPY(conf.disk_cache_dir2, dir.c_str());
		}
		create_cache_dir(conf.disk_cache_dir2);
		init_disk_cache(false);
		return sendErrPage("format disk cache done.");
	}
#endif
	if (strcmp(rq->sink->data.url->path, "/") == 0) {
		return sendMainPage();
	}
	hit = false;
	bool result = start_vhs(hit);
	if (hit) {
		return result;
	}
	result = start_access(hit);
	if(hit){
		return result;
	}
	return start_listen(hit);
}

