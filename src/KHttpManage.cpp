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
#include "KHttpLib.h"
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
#include "KBodyWStream.h"
#include "iconv.h"
#include "KProcessManage.h"
#include "KLogHandle.h"
#include "KUrlParser.h"
#include "KDsoExtendManage.h"
#include "KCache.h"
#include "kaddr.h"
#include "HttpFiber.h"
#include "KHttpServer.h"
#include "KConfigTree.h"
#include "extern.h"
#include "lang.h"
#include "KDefer.h"
#include "KChain.h"
#include "kmd5.h"
namespace kangle {
	KString get_connect_per_ip();
};
using namespace std;
using namespace kangle;
KPathHandler<kgl_request_handler> KHttpManage::handler(_KS(""));
KTHREAD_FUNCTION check_autoupdate(void* param);
void get_url_info(KSink* rq, KStringBuf& s) {
	if (rq->data.raw_url == NULL) {
		return;
	}
#ifdef HTTP_PROXY
	if (rq->data.meth == METH_CONNECT) {
		s << "CONNECT ";
		if (rq->data.raw_url->host) {
			s << rq->data.raw_url->host << ":" << rq->data.raw_url->port;
		}
		return;
	}
#endif//}}
	rq->data.raw_url->GetUrl(s, true);
	return;
}

bool kgl_connection_iterator(void* arg, KSink* rq) {
	KConnectionInfoContext* ctx = (KConnectionInfoContext*)arg;
	if (conf.max_connect_info > 0 && ctx->total_count > (int)conf.max_connect_info) {
		return false;
	}
	KSubVirtualHost* svh = static_cast<KSubVirtualHost*>(rq->data.opaque);
	if (ctx->vh != NULL && (svh == NULL || strcmp(svh->vh->name.c_str(), ctx->vh) != 0)) {
		return true;
	}
	ctx->total_count++;
	ctx->s << "rqs.push(new Array(";
	ctx->s << "'";
	if (ctx->translate) {
		char ips[MAXIPLEN];
		sockaddr_i* addr = rq->get_peer_addr();
		ksocket_sockaddr_ip(addr, ips, MAXIPLEN);
		ctx->s << "rq=" << (uint64_t)rq;
		ctx->s << ",peer=" << ips << ":" << ksocket_addr_port(addr);
	}
	ctx->s << "','" << rq->get_client_ip();
	ctx->s << "','" << (kgl_current_sec - rq->data.begin_time_msec / 1000);
	ctx->s << "','";
	if (ctx->translate) {
		ctx->s << klang[rq->get_state()];
	} else {
		ctx->s << rq->get_state();
	}
	ctx->s << "','";
	auto meths = KHttpKeyValue::get_method(rq->data.meth);
	ctx->s.write_all(meths->data, meths->len);
	ctx->s << "','";
	KStringBuf url;
	get_url_info(rq, url);
	if (url.size() > 0 && strchr(url.c_str(), '\'') == NULL) {
		ctx->s << url.c_str();
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
	KHttpHeader* referer = rq->data.find(kgl_expand_string("Referer"));
	if (referer) {
		ctx->s << KXml::encode(referer->buf + referer->val_offset);
	}
	ctx->s << "','";
	switch (rq->data.http_version) {
	case 0x300:
		ctx->s.write_all(_KS("3"));
		break;
	case 0x200:
		ctx->s.write_all(_KS("2"));
		break;
	case 0x100:
		ctx->s.write_all(_KS("1.0"));
		break;
	default:
		ctx->s.write_all(_KS("1.1"));
		break;
	}
	ctx->s << "',";
	ctx->s << rq->data.mark;
	ctx->s << ")); \n";
	return true;
}
KString endTag() {
	KStringBuf s;
	if (kconfig::need_reboot()) {
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
bool killProcess(KVirtualHost* vh) {
#ifndef HTTP_PROXY
	conf.gam->killAllProcess(vh);
#endif
	return true;
}
bool killProcess(KString process, KString user, int pid) {

	char* name = xstrdup(process.c_str());
	if (name == NULL) {
		return false;
	}
	char* p = strchr(name, ':');
	if (p == NULL) {
		xfree(name);
		return false;
	}
	*p = '\0';
	p++;
	if (strcasecmp(name, "api") == 0) {
		spProcessManage.killProcess(user.c_str(), pid);
	} else {
#ifdef ENABLE_VH_RUN_AS
		KCmdPoolableRedirect* rd = conf.gam->refsCmdRedirect(p);
		if (rd) {
			KProcessManage* pm = rd->getProcessManage();
			if (pm) {
				pm->killProcess(user.c_str(), pid);
			}
			rd->release();
		}
#endif
	}
	xfree(name);
	return true;
}
bool changeAdminPassword(KUrlValue* url, KString& errMsg) {
	auto admin_passwd = url->attribute["password"];
	if (admin_passwd.empty()) {
		errMsg = "change auth_type must reset password.please enter admin password";
		return false;
	}
	kconfig::update("admin"_CS, 0, nullptr, &url->attribute, kconfig::EvUpdate | kconfig::FlagCreate);
	return true;
}
bool KHttpManage::runCommand() {
	auto cmd = getUrlValue("cmd");

	if (cmd == "flush_log") {
#if 0
#ifndef HTTP_PROXY
		KHttpServerParser logParser;
		string configFile = conf.path;
		configFile += VH_CONFIG_FILE;
		//conf.gvm->clear();
		logParser.parse(configFile);
#endif
#endif
		return sendHttp("200");
#ifdef MALLOCDEBUG
	} else if (cmd == "dump_memory") {
		int min_time = atoi(getUrlValue("min_time").c_str());
		auto max_time_str = getUrlValue("max_time");
		int max_time = -1;
		if (max_time_str.size() > 0) {
			max_time = atoi(max_time_str.c_str());
		}
		dump_memory_leak(min_time, max_time);
		return sendHttp("200");
	} else if (cmd == "test_crash") {
		memcpy(0, "t", 1);
	} else if (cmd == "test_leak") {
		//警告: 这里只是测试使用，存在明显的内存泄漏
		//测试一个内存泄漏，以此测试内存泄漏工具是否正常工作
		int* a = new int;
		char* scode = strdup("200");
		return sendHttp(scode);
#endif
	} else if (cmd == "dump") {
#ifdef _WIN32
		coredump(GetCurrentProcessId(), GetCurrentProcess(), NULL);
		//{{ent
#ifndef KANGLE_FREE
		Sleep(2000);
		kthread_pool_start(crash_report_thread, NULL);
#endif//}}
		return sendHttp("200");
#endif
	} else if (cmd == "heapmin") {
#ifdef _WIN32
		_heapmin();
#endif
		return sendHttp("200");
	} else if (cmd == "cache") {
		INT64 csize, cdsize, hsize, hdsize;
		caculateCacheSize(csize, cdsize, hsize, hdsize);
		KStringBuf s;
		s << "<pre>";
		s << "csize:\t" << csize << "\n";
		s << "hsize:\t" << hsize << "\n";
		s << "cdsize:\t" << cdsize << "\n";
		s << "hdsize:\t" << hdsize << "\n";
		s << "</pre>";
		return sendHttp(s.str());
	} else if (cmd == "dump_refs_obj") {
		KStringBuf s;
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
	} else if (cmd == "dci") {
		KStringBuf s;
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
	KStringBuf s;
	unsigned i;

	const size_t max_extends = 5;
	const char* extends_header[max_extends] = { klang["single_server"]
#ifdef ENABLE_MULTI_SERVER
			,klang["multi_server"]
#else
			,NULL
#endif

#ifndef HTTP_PROXY
		,klang["api"],
		klang["cmd"]
#else
		,NULL,NULL
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
		if (item == i) {
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
		conf.gam->acserverList(s, getUrlValue("name"));
	} else if (item == 1) {
#ifdef ENABLE_MULTI_SERVER
		conf.gam->macserverList(s, getUrlValue("name"));
#endif
	} else if (item == 2) {
		KString name;
		if (getUrlValue("action") == "edit") {
			name = getUrlValue("name");
		}
		conf.gam->apiList(s, name);
#ifdef ENABLE_VH_RUN_AS
	} else if (item == 3) {
		KString name;
		if (getUrlValue("action") == "edit") {
			name = getUrlValue("name");
		}
		conf.gam->cmdList(s, name);
#endif
	} else if (item == 4) {
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
bool KHttpManage::sendXML(const char* buf, bool addXml) {
	if (!addXml) {
		return sendHttp(buf, strlen(buf), "text/xml");
	}
	stringstream s;
	s << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" << buf;
	return sendHttp(s.str().c_str(), s.str().size(), "text/xml");
}
bool KHttpManage::config() {

	size_t i = 0;

	string config_header[] = { LANG_SERVICE, LANG_CACHE, LANG_LOG,LANG_RS_LIMIT,klang["data_exchange"], LANG_OTHER_CONFIG, LANG_MANAGE_ADMIN };
	size_t max_config = sizeof(config_header) / sizeof(string);
	KStringBuf s;
	unsigned item = atoi(getUrlValue("item").c_str());
#ifdef KANGLE_ETC_DIR
	auto file_name = KANGLE_ETC_DIR;
#else
	auto file_name = conf.path + "/etc";
#endif
	bool canWrite = true;
	//	stringstream s;
	conf.admin_lock.Lock();
	file_name += CONFIG_FILE;
	FILE* fp = fopen(file_name.c_str(), "a+");
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
#if 0
			if (i == 1) {
				s << "<a href='/cache.html'>" << config_header[i] << "</a>";
			} else
#endif
				s << "<a href=/config?item=" << i << ">" << config_header[i] << "</a>";
		}
		s << "] ";
	}
	s << "<br><hr>";
	s << "<form action=/configsubmit?item=" << item << " method=post>";

	if (item == 0) {
		s << "<table border=0><tr><td valign=top>";
		s << klang["config_listen"] << ":";
		s << "<table border=1>";
		s << "<tr><td>" << LANG_OPERATOR << "</td><td>" << LANG_IP << "</td><td>" << LANG_PORT << "</td><td>" << klang["listen_type"] << "</td></tr>";
		for (auto it = conf.services.begin(); it != conf.services.end(); ++it) {
			int index = 0;
			for (auto&& lh : (*it).second) {
				if (lh) {
					s << "<tr><td>";
					s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/deletelisten?file=" << (*it).first << "&id=";
					s << index << "';}\">" << LANG_DELETE << "</a>][<a href='/newlistenform?action=edit&file=" << (*it).first << "&id=" << index << "'>" << LANG_EDIT << "</a>]</td>";
					s << "<td>" << lh->ip << "</td>";
					s << "<td>" << lh->port << "</td>";
					s << "<td>" << getWorkModelName(lh->model) << "</td>";
					s << "</tr>";
				}
				++index;
			}
		}
		s << "</table>";
		s << "[<a href='/newlistenform'>" << klang["new_listen"] << "</a>]<br>";
		s << klang["connect_time_out"] << ":<input name='connect_time_out' size=5 value='" << conf.connect_time_out << "'><br>";
		s << LANG_TIME_OUT << ":<input name='time_out' size=5 value='" << conf.time_out << "'><br>";
		s << klang["worker_thread"] << ":<select name='worker_thread'>";
		for (int i = 0; i < 10; i++) {
			int count = (1 << i) / 2;
			s << "<option value='" << count << "' ";
			if (count == conf.select_count) {
				s << "selected";
			}
			s << ">" << count << "</option>";
		}
		s << "</select>";
		s << "</td><td valign=top>";
		s << "\n" << klang["success_listen"] << ":<table border=1>";
		s << "<tr><td>" << LANG_IP << "</td><td>" << LANG_PORT;
		s << "</td><td>" << klang["listen_type"] << "</td><td>" << klang["protocol"] << "</td><td>flags</td><td>global</td><td>dynamic</td><td>" << LANG_REFS << "</td></tr>";
		conf.gvm->GetListenHtml(s);
		s << "</table>";
		s << "</tr></table>";
	} else if (item == 1) {
		//default
		s << klang["lang_default_cache"] << "<select name='default'><option value=1 ";
		if (conf.default_cache > 0) {
			s << "selected";
		}
		s << " > " << LANG_ON << "</option><option value=0 ";
		if (conf.default_cache <= 0) {
			s << "selected";
		}
		s << " >" << LANG_OFF << "</option></select><br>";
		s << "" << LANG_TOTAL_MEM_CACHE << ":<input type=text name=memory size=8 value='" << get_size(conf.mem_cache) << "'><br>";
#ifdef ENABLE_DISK_CACHE
		s << LANG_TOTAL_DISK_CACHE << ":<input type=text name=disk size=8 value='" << get_size(conf.disk_cache) << (conf.disk_cache_is_radio ? "%" : "") << "'><br>";
		s << klang["disk_cache_dir"] << ":<input type=text name=disk_dir value='" << conf.disk_cache_dir2 << "'>[<a href='/format_disk_cache_dir.km'>" << klang["format_disk_cache"] << "</a>]<br>";
		s << klang["disk_work_time"] << ":<input type=text name=disk_work_time value='" << conf.disk_work_time << "'><br>";
#endif
		s << LANG_MAX_CACHE_SIZE << ":<input type=text name=max_cache_size size=6 value='" << get_size(conf.max_cache_size) << "'><br>";
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
		s << LANG_LOG_ROTATE_TIME << "<input type=text name=rotate_time value=\"" << conf.log_rotate << "\"></br>\n";
		s << klang["log_rotate_size"] << ":<input type=text name=rotate_size size=6 value=\"" << get_size(conf.log_rotate_size) << "\"></br>\n";
		s << klang["error_rotate_size"] << ":<input type=text name='error_rotate_size' size=6 value=\"" << get_size(conf.error_rotate_size) << "\"></br>\n";
		s << klang["log_level"] << "<input type=text name=level value='" << conf.log_level << "'></br>\n";
		s << klang["logs_day"] << "<input type=text name='logs_day' value='" << conf.logs_day << "'></br>\n";
		s << klang["logs_size"] << "<input type=text name='logs_size' value='" << get_size(conf.logs_size) << "'></br>\n";
		s << "log_radio:<input type=text name='radio' value='" << conf.log_radio << "'></br>\n";
		s << "<input type=checkbox name='log_handle' value='1' ";
		if (conf.log_handle) {
			s << "checked";
		}
		s << ">" << klang["log_handle"] << "</br>\n";
		s << klang["access_log_handle"] << "<input type=text size='30' name='access_log_handle' value='" << conf.logHandle << "'></br>\n";
		s << klang["log_handle_concurrent"] << "<input type=text name='log_handle_concurrent' value='" << conf.maxLogHandle << "'></br>\n";
	} else if (item == 3) {
		s << klang["max_connection"] << ": <input type=text size=5 name=max value=" << conf.max << "><br>";

		s << LANG_TOTAL_THREAD_EACH_IP << ":<input type=text size=3 name=max_per_ip value=" << conf.max_per_ip << "><br>";
		s << klang["keep_alive_count"] << ":<input name='max_keep_alive' size=6 value='" << conf.keep_alive_count << "'>" << "<br>";
#ifdef ENABLE_BLACK_LIST
		s << "<input type=checkbox name=per_ip_deny value=1 ";
		if (conf.per_ip_deny) {
			s << "checked";
		}
		s << ">per_ip_deny<br>";
#endif
		s << klang["min_free_thread"] << ":<input type=text size=3 name=min_free_thread value='" << conf.min_free_thread << "'><br>";

#ifdef ENABLE_ADPP
		s << klang["process_cpu_usage"]
			<< ":<input type=text size=3 name='process_cpu_usage' value="
			<< conf.process_cpu_usage << "><br>";
#endif


		s << klang["dns_worker"] << ":<input type=text size=4 name='worker_dns' value='" << conf.worker_dns << "'><br>";
		s << "fiber stack size:<input type=text name='fiber_stack_size' size=4 value='" << get_size(http_config.fiber_stack_size) << "'><br>";
	} else if (item == 4) {
		//data exchange
#ifdef ENABLE_TF_EXCHANGE	
		s << klang["max_post_size"] << ":<input name='max_post_size' size='4' value='" << get_size(conf.max_post_size) << "'><br>\n";
#endif
		s << klang["io_worker"] << ":<input type=text size=4 name='worker_io' value='" << conf.worker_io << "'><br>";
		s << "max_io:<input type=text size=4 name='max_io' value='" << conf.max_io << "'><br>";
		s << "io_buffer:" << get_size(conf.io_buffer) << "<br>";
		s << "upstream sign : <code title='len=" << conf.upstream_sign_len << "'>" << conf.upstream_sign << "</code><br>\n";
		s << "<!-- read_hup=" << conf.read_hup << " -->\n";
	} else if (item == 5) {
		s << klang["lang_only_gzip_cache"] << "<select name=only_compress_cache><option value=1 ";
		if (conf.only_compress_cache == 1) {
			s << "selected";
		}
		s << " > " << LANG_ON << "</option><option value=0 ";
		if (conf.only_compress_cache != 1) {
			s << "selected";
		}
		s << " >" << LANG_OFF << "</option></select><br>";

		s << "min compress length:<input name='min_compress_length' size=6 value='" << conf.min_compress_length << "'><br>";
		s << "<a href=# title='(0-9,0=disable)'>gzip level</a><input name=gzip_level size=3 value='" << conf.gzip_level << "'>";
#ifdef ENABLE_BROTLI
		s << "<a href=# title='(0-9,0=disable)'>br level</a><input name=br_level size=3 value='" << conf.br_level << "'>";
#endif
#ifdef ENABLE_ZSTD
		s << "<a href=# title='(0-9,0=disable)'>zstd level</a><input name=zstd_level size=3 value='" << conf.zstd_level << "'>";
#endif
		s << "<br>";
#ifdef KANGLE_ENT
		s << klang["server_software"]
			<< "<input type=text name='server_software' value='"
			<< conf.server_software << "'><br>";
#endif
		s << klang["hostname"] << "<input type=text name='hostname' value='";
		if (*conf.hostname) {
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
#ifdef ENABLE_FATBOY
		s << klang["bl_time"] << ":" << conf.bl_time << "<br>";
		s << klang["wl_time"] << ":" << conf.wl_time << "<br>";
#endif
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
	} else if (item == 6) {
		s << LANG_ADMIN_USER << ":<input name=user value='"
			<< conf.admin_user << "'><br>";
		s << LANG_ADMIN_PASS
			<< ":<input name=password autocomplete='off' type=password value=''><br>";
		s << klang["auth_type"];
		for (i = 0; i < TOTAL_AUTH_TYPE; i++) {
			s << "<input type=radio name='auth_type' value='" << KHttpAuth::buildType((int)i) << "' ";
			if ((unsigned)conf.auth_type == i) {
				s << "checked";
			}
			s << ">" << KHttpAuth::buildType((int)i) << " ";
		}
		s << "<br>";
		s << "" << LANG_ADMIN_IPS << ":<input name=admin_ips value='";
		for (i = 0; i < conf.admin_ips.size(); i++) {
			s << conf.admin_ips[i] << "|";
		}
		s << "'><br>";
	}
	s << "<br><input type=submit value='" << LANG_SUBMIT << "'></form>" << endTag() << "</body></html>";
	conf.admin_lock.Unlock();
	return sendHttp(s.str());
}
bool KHttpManage::configsubmit() {
	//	size_t i;
	KStringBuf url;
	size_t item = atoi(removeUrlValue("item").c_str());
	if (item == 0) {
		KXmlAttribute attr;
		attr.emplace("rw", getUrlValue("time_out"));
		attr.emplace("connect", getUrlValue("connect_time_out"));
		kconfig::update("timeout"_CS, 0, nullptr, &attr, kconfig::EvUpdate | kconfig::FlagCreate);
		//kconfig::update("keep_alive_count"_CS, 0, &getUrlValue("keep_alive_count"), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("worker_thread"_CS, 0, &getUrlValue("worker_thread"), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		int worker_thread = atoi(getUrlValue("worker_thread").c_str());
		if (worker_thread != conf.select_count) {
			conf.select_count = worker_thread;
			kconfig::set_need_reboot();
		}
	} else if (item == 1) {
		kconfig::update("cache"_CS, 0, nullptr, &urlValue.attribute, kconfig::EvUpdate | kconfig::FlagCreate);
	} else if (item == 2) {
		auto v = removeUrlValue("access_log"_CS);
		kconfig::update("access_log"_CS, 0, &v, nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		v = removeUrlValue("access_log_handle"_CS);
		kconfig::update("access_log_handle"_CS, 0, &v, nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		v = removeUrlValue("log_handle_concurrent"_CS);
		kconfig::update("log_handle_concurrent"_CS, 0, &v, nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("log"_CS, 0, nullptr, &urlValue.attribute, kconfig::EvUpdate | kconfig::FlagCreate);
	} else if (item == 3) {
		KXmlAttribute connect_attr;
		connect_attr.emplace("max"_CS, getUrlValue("max"));
		connect_attr.emplace("max_per_ip"_CS, getUrlValue("max_per_ip"));
		connect_attr.emplace("per_ip_deny"_CS, getUrlValue("per_ip_deny"));
		connect_attr.emplace("max_keep_alive"_CS, getUrlValue("max_keep_alive"));
		kconfig::update("connect"_CS, 0, nullptr, &connect_attr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("min_free_thread"_CS, 0, &getUrlValue("min_free_thread"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
#ifdef ENABLE_ADPP
		kconfig::update("process_cpu_usage"_CS, 0, &getUrlValue("process_cpu_usage"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
#endif
		KXmlAttribute dns_attr;
		dns_attr.emplace("worker"_CS, getUrlValue("worker_dns"));
		kconfig::update("dns"_CS, 0, nullptr, &dns_attr, kconfig::EvUpdate | kconfig::FlagCreate);

		KXmlAttribute fiber_attr;
		fiber_attr.emplace("stack_size"_CS, getUrlValue("fiber_stack_size"));
		kconfig::update("fiber"_CS, 0, nullptr, &fiber_attr, kconfig::EvUpdate | kconfig::FlagCreate);

	} else if (item == 4) {
		//data exchange
		KXmlAttribute io_attr;
		io_attr.emplace("worker"_CS, getUrlValue("worker_io"));
		io_attr.emplace("max"_CS, getUrlValue("max_io"));
		kconfig::update("io"_CS, 0, nullptr, &io_attr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("max_post_size"_CS, 0, &getUrlValue("max_post_size"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
	} else if (item == 5) {
#ifdef MALLOCDEBUG
		kconfig::update("mallocdebug"_CS, 0, &getUrlValue("mallocdebug"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
#endif
#ifdef KSOCKET_UNIX
		kconfig::update("unix_socket"_CS, 0, &getUrlValue("unix_socket"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
#endif
		kconfig::update("path_info"_CS, 0, &getUrlValue("path_info"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		KXmlAttribute compress_attr;
		compress_attr.emplace("gzip_level"_CS, getUrlValue("gzip_level"));
#ifdef ENABLE_BROTLI
		compress_attr.emplace("br_level"_CS, getUrlValue("br_level"));
#endif
#ifdef ENABLE_ZSTD
		compress_attr.emplace("zstd_level"_CS, getUrlValue("zstd_level"));
#endif
		compress_attr.emplace("only_cache"_CS, getUrlValue("only_compress_cache"));
		compress_attr.emplace("min_length"_CS, getUrlValue("min_compress_length"));
		kconfig::update("compress"_CS, 0, nullptr, &compress_attr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("server_software"_CS, 0, &getUrlValue("server_software"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		kconfig::update("hostname"_CS, 0, &getUrlValue("hostname"_CS), nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
#ifdef ENABLE_FATBOY
		//KXmlAttribute fw_attr;
		//fw_attr.emplace("bl_time"_CS, getUrlValue("bl_time"));
		//fw_attr.emplace("wl_time"_CS, getUrlValue("wl_time"));
#endif
	} else if (item == 6) {
		KString errMsg;
		if (!changeAdminPassword(&urlValue, errMsg)) {
			return sendErrPage(errMsg.c_str());
		}
	}
	url << "/config?item=" << item;
	return sendRedirect(url.str().c_str());
}

KHttpManage::KHttpManage() :KFetchObject(0){
	userType = USER_TYPE_UNAUTH;
	rq = NULL;
	postData = NULL;
	postLen = 0;
}
KHttpManage::~KHttpManage() {
	if (postData) {
		free(postData);
	}
}
KString KHttpManage::removeUrlValue(const KString& name) {
	auto it = urlValue.attribute.find(name);
	if (it == urlValue.attribute.end()) {
		return "";
	}
	KString value(std::move((*it).second));
	urlValue.attribute.erase(it);
	return value;
}
const KString& KHttpManage::getUrlValue(const KString& name) {
	return urlValue.attribute[name];
}
#if 0
bool KHttpManage::parseUrlParam(char* param, size_t len) {
	char* name;
	char* value;
	char* tmp;
	char* msg;
	char* ptr;

	char* end = (char*)kgl_mempbrk(param, (int)len, _KS("\r\n"));
	if (end) {
		assert((int)(end - param) <= len);
		*end = '\0';
	}
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
			//urlParam.emplace(name, value);
			//urlParam.insert(pair<string, string>(name, value));
			urlValue.add(name, value);
		}

	}
	return true;
}
#endif
bool KHttpManage::parseUrl(const char* url) {
	return 	urlValue.parse(url);
}
bool KHttpManage::sendHttp(const char* msg, INT64 content_length, const char* content_type, const char* add_header, int max_age) {
	KStringBuf s;
	out->f->write_status(out->ctx, STATUS_OK);
	if (content_type) {
		out->f->write_unknow_header(out->ctx, kgl_expand_string("Content-Type"), content_type, (hlen_t)strlen(content_type));
	} else {
		out->f->write_unknow_header(out->ctx, kgl_expand_string("Content-Type"), kgl_expand_string("text/html; charset=utf-8"));
	}
	if (max_age == 0) {
		out->f->write_unknow_header(out->ctx, kgl_expand_string("Cache-control"), kgl_expand_string("no-cache,no-store"));
	} else {
		s << "public,max_age=" << max_age;
		out->f->write_unknow_header(out->ctx, kgl_expand_string("Cache-control"), s.buf(), s.size());
	}
	out->f->write_unknow_header(out->ctx, _KS("x-gzip"), _KS("1"));
	KBIT_SET(rq->ctx.obj->index.flags, ANSW_LOCAL_SERVER);
	kgl_response_body body;
	if (content_length < 0 && msg) {
		content_length = strlen(msg);
	}
	KGL_RESULT result = out->f->write_header_finish(out->ctx, content_length, &body);
	if (result != KGL_OK) {
		return false;
	}
	if (msg) {
		body.f->close(body.ctx, body.f->write(body.ctx, msg, (int)content_length));
	} else {
		body.f->close(body.ctx, KGL_OK);
	}
	return true;
}
bool KHttpManage::sendHttp(const KString& msg) {
	return sendHttp(msg.c_str(), msg.size());
}
void KHttpManage::sendTest() {
#if 0
	map<string, string>::iterator it;
	for (it = urlParam.begin(); it != urlParam.end(); it++) {
		printf("name:");
		printf("%s", (*it).first.c_str());
		printf(" value:");
		printf("%s", (*it).second.c_str());
		printf("\n");
	}
#endif
}
bool KHttpManage::reboot() {
	KStringBuf s;
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
bool KHttpManage::sendErrPage(const char* err_msg, int close_flag) {
	KStringBuf s;
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
	s << "<html><head><title>" << PROGRAM_NAME << "(" << VER_ID << ") " << LANG_MANAGE << "</title><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << "<img border=0 src='/logo.gif' alt='logo'>";
	s << "<table width='100%'><tr><td height=30><a href=/main target=mainFrame>" << LANG_HOME << "</a></td></tr>";
	s << "<tr><td height=30><a href='/accesslist?access_type=0' target=mainFrame>" << klang["lang_requestAccess"] << "</a></td></tr>";
	s << "<tr><td height=30><a href='/accesslist?access_type=1' target=mainFrame>" << klang["lang_responseAccess"] << "</a></td></tr>";
	s << "<tr><td height=30><a href=/acserver target=mainFrame>" << klang["extends"] << "</a></td></tr>\n";
	s << "<tr><td height=30><a href='/vhslist' target=mainFrame>" << LANG_VHS << "</a></td></tr>\n";
#ifndef HTTP_PROXY
	s << "<tr><td height=30><a href='/process' target=mainFrame>" << klang["process"] << "</a></td></tr>\n";
#endif
#ifdef ENABLE_WRITE_BACK
	//s << "<tr><td height=30><a href=/writeback target=mainFrame>" << LANG_WRITE_BACK << "</a></td></tr>";
#endif
	s << "<tr><td height=30><a href=/connect_per_ip target=mainFrame>" << LANG_CONNECT_PER_IP << "</a></td></tr>";
	s << "<tr><td height=30><a href=/connection target=mainFrame>" << LANG_CONNECTION << "</a></td></tr>";
	s << "<tr><td height=30><a href='/config' target=mainFrame>" << LANG_CONFIG << "</a></td></tr>";
	s << "<tr><td height=30><a href=\"javascript:if(confirm('really reboot')){ window.parent.location='/reboot';}\">" << klang["reboot"] << "</a></td></tr>";
	s << "</table></body></html>";
	return sendHttp(s.str().c_str());
}
bool KHttpManage::sendMainFrame() {
	KStringBuf s;
	char buf[256];
	INT64 total_mem_size = 0, total_disk_size = 0;
	int mem_count = 0, disk_count = 0;
	time_t total_run_time = kgl_current_sec - kgl_program_start_sec;
	cache.getSize(total_mem_size, total_disk_size, mem_count, disk_count);
	s << "<html><head><title>" << PROGRAM_NAME << "(" << VER_ID << ") ";
	s << LANG_INFO << "</title><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << "<table width='98%'><tr><td>";
	s << "<h3>" << klang["program_info"] << "</h3><table border=0>";
	s << "<tr><td>" << klang["version"] << "</td><td>" << VERSION << "(" << getServerType() << ") ";
	if (serial > 0) {
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
	s << LANG_TOTAL_MEM_CACHE << "</td><td>" << mem_count << "</td><td>" << get_human_size(double(total_mem_size), buf, sizeof(buf)) << "</td></tr>";
#ifdef ENABLE_DISK_CACHE
	s << "<tr><td>";
	s << LANG_TOTAL_DISK_CACHE << "</td><td>" << disk_count << "</td><td>" << get_human_size(double(total_disk_size), buf, sizeof(buf)) << "</td></tr>";
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
	s << total_run_time << " " << LANG_SECOND << ". pid: " << getpid();
	s << "<h3>" << klang["load_info"] << "</h3>";
	s << "<table>";
	//connect
	s << "<tr><td>" << LANG_CONNECT_COUNT << "</td><td>" << total_connect << "</td></tr>\n";
	//thread
	int worker_count, free_count;
	kthread_get_count(&worker_count, &free_count);
	s << "<tr><td>fiber</td><td>" << kfiber_get_count() << "</td></tr>\n";
	s << "<tr><td>" << LANG_WORKING_THREAD << "</td><td>" << worker_count << "</td></tr>\n";
	s << "<tr><td>" << LANG_FREE_THREAD << "</td><td>" << free_count << "</td></tr>\n";
	s << "<tr><td>" << klang["io_worker_info"] << "</td><td>" << conf.ioWorker->worker << "/" << conf.ioWorker->queue << "</td></tr>\n";
	s << "<tr><td>" << klang["dns_worker_info"] << "</td><td>" << conf.dnsWorker->worker << "/" << conf.dnsWorker->queue << "</td></tr>\n";
	s << "<tr><td>addr cache:</td><td>" << kgl_get_addr_cache_count() << "</td></tr>\n";
#ifdef ENABLE_DB_DISK_INDEX
	s << "<tr><td>dci queue:</td><td>" << (dci ? dci->getWorker()->queue : 0) << "</td></tr>\n";
	s << "<tr><td>dci mem:</td><td>" << (dci ? dci->memory_used() : 0) << "</td></tr>\n";
#endif
	s << "</table>\n";
	s << "<h3>" << klang["selector"] << "</h3>";
	s << "<table>";
	s << "<tr><td>" << LANG_NAME << "</td><td>" << selector_manager_event_name() << " " << kfiber_powered_by() << "</td></tr>\n";
	s << "<tr><td>" << klang["worker_thread"] << "</td><td>" << get_selector_count() << "</td></tr>\n";
	s << "</table>";

	s << "</td><td valign=top align=right>";
	s << "<form action='/changelang' method='post' target=_top><div align=right>";
	s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_config';}\">" << klang["reload_config"] << "</a>] ";
	//s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_vh';}\">" << klang["reload_vh"] << "</a>] ";
	s << klang["lang"] << ":<select name='lang'>";
	std::vector<KString> langNames;
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
	KString css = klang["css"];
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
bool KHttpManage::sendRedirect(const char* newUrl) {
	rq->response_status(STATUS_FOUND);
	rq->response_header(kgl_expand_string("Content-Length"), 0);
	rq->response_connection();
	rq->response_header(kgl_expand_string("Location"), newUrl, (hlen_t)strlen(newUrl));
	return rq->start_response_body(0);
}
bool matchManageIP(const char* ip, std::vector<KString>& ips, KString& matched_ip) {
	for (size_t i = 0; i < ips.size(); i++) {
		if (ips[i][0] == '~') {
			if (strcasecmp(ips[i].c_str() + 1, ip) == 0) {
				matched_ip = ips[i];
				return true;
			}
			continue;
		}
		if (ips[i] == "*" || strcasecmp(ips[i].c_str(), ip) == 0) {
			matched_ip = ips[i];
			return true;
		}
	}
	return false;
}
char* KHttpManage::parsePostFile(int& len, KString& fileName) {
	KHttpHeader* header = rq->sink->data.get_header();
	char* boundary = NULL;
	if (postData == NULL) {
		return NULL;
	}
	while (header) {
		if (kgl_is_attr(header, _KS("Content-Type"))) {
			boundary = strstr(header->buf + header->val_offset, "boundary=");
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
	int boundary_len = (int)strlen(boundary);
	char* pstr = strstr(postData, boundary);
	if (pstr == NULL) {
		return NULL;
	}
	pstr = strchr(pstr, '\n');
	if (pstr == NULL)
		return NULL;
	pstr += 1;
	char* next_line;
	char filename[256];
	char* no_dir;
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
			char* org_file = strrchr(pstr, ';');
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
	int past_len = (int)(pstr - postData);
	int data_len = postLen;
	data_len -= past_len;
	past_len = 0;
	char* may_boundary;
	string boundary_end = "\n--";
	boundary_end += boundary;
	for (;;) {
		if (data_len < boundary_len) {
			goto error;
		}
		may_boundary = (char*)memchr(pstr + past_len, boundary_end[0],
			data_len - past_len);
		if (may_boundary == NULL) {
			goto error;
		}
		past_len = (int)(may_boundary - pstr);
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
	if (rq->sink->data.left_read <= 0) {
		return;
	}
	char* buffer = NULL;
	if (rq->sink->data.left_read > MAX_POST_SIZE) {
		fprintf(stderr, "post size is too big\n");
		return;
	}
	buffer = (char*)xmalloc((int)(rq->sink->data.left_read + 1));
	postLen = (int)rq->sink->data.left_read;
	char* str = buffer;
	int length = 0;
	while (rq->sink->data.left_read > 0) {
		length = rq->read(str, (int)rq->sink->data.left_read);
		if (length <= 0) {
			free(buffer);
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return;
		}
		str += length;
	}

	buffer[postLen] = 0;
	postData = buffer;
}
bool checkManageLogin(KHttpRequest* rq) {
	kgl_str_t result;
	if (rq->get_http_value(_KS(X_REAL_IP_HEADER), &result)) {
		rq->sink->shutdown();
		return false;
	}
#ifdef KSOCKET_UNIX
	if (KBIT_TEST(rq->sink->get_server_model(), WORK_MODEL_UNIX)) {
		return true;
	}
#endif
	char ips[MAXIPLEN];
	rq->sink->get_peer_ip(ips, sizeof(ips));
	if (strcmp(ips, "127.0.0.1") != 0) {
		auto manage_sec_file = conf.path + "manage.sec";
		KFile file;
		if (file.open(manage_sec_file.c_str(), fileRead)) {
			rq->sink->shutdown();
			return false;
		}
	}
	if (!conf.admin_ips.empty()) {
		KString matched_ip;
		if (!matchManageIP(ips, conf.admin_ips, matched_ip)) {
			return false;
		}
		if (matched_ip[0] == '~') {
			return true;
		}
	}
	if (rq->auth == NULL) {
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
bool KHttpManage::start_listen(bool& hit) {
	KString err_msg;
	if (strcmp(rq->sink->data.url->path, "/deletelisten") == 0) {
		hit = true;
		int id = atoi(getUrlValue("id").c_str());
		KString file(getUrlValue("file"));
		kconfig::remove(file.str(), "listen"_CS, id);
		return sendRedirect("/config");
	}
	if (strcmp(rq->sink->data.url->path, "/newlisten") == 0) {
		hit = true;
		auto action = removeUrlValue("action");
		KString file(removeUrlValue("file"));
		if (file.empty()) {
			file = "default";
		}
		auto id = removeUrlValue("id");
		auto xml = kconfig::new_xml(_KS("listen"));
		xml->attributes().swap(urlValue.attribute);
		if (action == "edit") {
			kconfig::update(file.str(), "listen"_CS, atoi(id.c_str()), xml.get(), kconfig::EvUpdate);
		} else {
			kconfig::add(file.str(), "listen"_CS, khttpd::last_pos, xml.get());
		}
		return sendRedirect("/config");
	}
	if (strcmp(rq->sink->data.url->path, "/newlistenform") == 0) {
		hit = true;
		KStringBuf s;
		int id = atoi(getUrlValue("id").c_str());
		const KListenHost* host = nullptr;
		auto file = getUrlValue("file");
		if (getUrlValue("action") == "edit") {
			auto it = conf.services.find(file);
			if (it == conf.services.end()) {
				return sendErrPage("cann't find such listen");
			}
			try {
				host = (*it).second.at(id).get();
			} catch (std::out_of_range) {
				return sendErrPage("cann't find such listen");
			}
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
			<< "&id=" << id << "&file=" << file << "' method='post'>\n";
		s << "<table>";
		s << "<tr><td>" << LANG_IP << ":</td><td><input name='ip' value='"
			<< (host ? host->ip : "*") << "'></td></tr>";
		s << "<tr><td>" << LANG_PORT << ":</td><td><input name='port' value='"
			<< (host ? host->port : "80") << "'>";
		s << "</td></tr>";
		s << "<tr><td>" << klang["listen_type"] << ":</td><td>";
		static const char* model[] = { "http", "manage"
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
		for (size_t i = 0; i < sizeof(model) / sizeof(char*); i++) {
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
		if (host == NULL || !KBIT_TEST(host->model, WORK_MODEL_SSL)) {
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
#if defined(ENABLE_HTTP2) && defined(TLSEXT_TYPE_next_proto_neg)
		s << "<input type='checkbox' name='http2' value='1' ";
		if (host == NULL || KBIT_TEST(host->alpn, KGL_ALPN_HTTP2)) {
			s << "checked";
		}
		s << ">http2";
#ifdef ENABLE_HTTP3
		s << "<input type='checkbox' name='http3' value='1' ";
		if (host && KBIT_TEST(host->alpn, KGL_ALPN_HTTP3)) {
			s << "checked";
		}
		s << ">http3";
#endif
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
		s << "<input type='checkbox' name='early_data' value='1' ";
		if (host && host->early_data) {
			s << "checked";
		}
		s << ">early_data";
#endif
		s << "<input type='checkbox' name='reject_nosni' value='1' ";
		if (host && host->reject_nosni) {
			s << "checked";
		}
		s << ">reject_nosni";
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
bool KHttpManage::start_access(bool& hit) {
	hit = true;
	int type = !!atoi(getUrlValue("access_type").c_str());
	KStringBuf accesslist;
	auto vh_name = getUrlValue("vh");
	KSafeVirtualHost vh;
	KSafeAccess access(kaccess[type]->add_ref());
#ifndef HTTP_PROXY
	if (!vh_name.empty()) {
		vh.reset(conf.gvm->refsVirtualHostByName(vh_name));
		if (!vh) {
			return sendHttp("cann't find such vh");
		}
		if (vh->user_access.empty()) {
			return sendHttp("vh do not support user access");
		}
		access = vh->access[type];
	}
#endif
	accesslist << "/accesslist?access_type=" << getUrlValue("access_type") << "&vh=" << getUrlValue("vh");
	if (strcmp(rq->sink->data.url->path, "/accesslist") == 0) {
		KStringBuf s;
		if (vh) {
			conf.gvm->getHtml(s, vh_name, type + 6, urlValue);
		} else if (access) {
			s << access->htmlAccess();
		}
		return sendHttp(s.str());
	}
	if (strcmp(rq->sink->data.url->path, "/tableadd") == 0) {
		KStringBuf name;
		KStringBuf path;
		KStringBuf file;
		auto flag = kconfig::EvUpdate | kconfig::FlagCreate | kconfig::FlagCopyChilds | kconfig::FlagCreateParent;
		if (vh && vh->has_user_access()) {
			vh->get_access_file(file);
		}
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		name << "table@"_CS << getUrlValue("table_name");
		path << access->get_qname() << "/"_CS << name;
		auto xml = kconfig::new_xml(name.c_str(), name.size());
		if (!file.empty()) {
			kconfig::update(file.str().str(), path.str().str(), 0, xml.get(), flag);
		} else {
			kconfig::update(path.str().str(), 0, xml.get(), flag);
		}
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/tabledel") == 0) {
		KStringBuf path;
		KStringBuf file;
		if (vh && vh->has_user_access()) {
			vh->get_access_file(file);
		}
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname() << "/table@"_CS << getUrlValue("table_name");
		if (!file.empty()) {
			kconfig::remove(file.str().str(), path.str().str(), 0);
		} else {
			kconfig::remove(path.str().str(), 0);
		}
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/accesschangefirst") == 0) {
		KStringBuf path;
		KStringBuf file;
		if (vh && vh->has_user_access()) {
			vh->get_access_file(file);
		}
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname();
		auto xml = kconfig::new_xml(access->get_qname());
		KAccess::build_action_attribute(xml->attributes(), urlValue);
		if (!file.empty()) {
			kconfig::update(file.str().str(), path.str().str(), 0, xml.get(), kconfig::EvUpdate|kconfig::FlagCopyChilds|kconfig::FlagCreate);
		} else {
			kconfig::update(path.str().str(), 0, xml.get(), kconfig::EvUpdate|kconfig::FlagCopyChilds|kconfig::FlagCreate);
		}
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/delchain") == 0) {
		KStringBuf path;
		auto file = getUrlValue("file");
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname() << "/table@"_CS << getUrlValue("table_name") << "/chain";
		kconfig::remove(file.str(), path.str().str(), urlValue.attribute.get_int("id"));
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/addchain") == 0) {
		KStringBuf path;
		auto file = getUrlValue("file");
		if (!file && vh && vh->has_user_access()) {
			KStringBuf tfile;
			vh->get_access_file(tfile);
			tfile.str().swap(file);
		}
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname() << "/table@" << getUrlValue("table_name") << "/chain"_CS;
		auto xml = kconfig::new_xml("chain"_CS);
		xml->attributes().emplace("action"_CS, "continue"_CS);
		if (file) {
			kconfig::update(file.str(), path.str().str(), (uint32_t)urlValue.attribute.get_int("id"), xml.get(), kconfig::EvNew);
		} else {
			kconfig::update(path.str().str(), (uint32_t)urlValue.attribute.get_int("id"), xml.get(), kconfig::EvNew);
		}
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/editchain") == 0) {
		KStringBuf path;
		bool mark = false;
		if (getUrlValue("mark") == "1") {
			mark = true;
		}
		auto file = getUrlValue("file");
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname() << "/table@" << getUrlValue("table_name") << "/chain"_CS;
		auto id = getUrlValue("id");
		if (getUrlValue("modelflag") == "1") {
			path << "#"_CS << id << (mark ? "/mark"_CS : "/acl"_CS);
			KStringBuf url;
			auto acl_xml = kconfig::new_xml(mark ? "mark"_CS : "acl"_CS);
			acl_xml->attributes().emplace("module"_CS, getUrlValue("modelname"));
			auto result = kconfig::update(file.str(), path.str().str(), (uint32_t)urlValue.attribute.get_int("model"), acl_xml.get(), kconfig::EvNew);
			url << "/editchainform?access_type=" << getUrlValue("access_type")
				<< "&table_name=" << getUrlValue("table_name")
				<< "&file=" << getUrlValue("file")
				<< "&index=" << getUrlValue("index")
				<< "&id=" << getUrlValue("id")
				<< "&vh=" << getUrlValue("vh");
			return sendRedirect(url.c_str());
		}
		auto result = kconfig::update(file.str(), path.str().str(), (uint32_t)atoi(id.c_str()), KChain::to_xml(urlValue).get(), kconfig::EvUpdate);
		return sendRedirect(accesslist.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/delmodel") == 0) {
		bool mark = false;
		if (getUrlValue("mark") == "1") {
			mark = true;
		}
		KStringBuf path;
		auto file = getUrlValue("file");
		if (vh_name && strncmp(file.c_str(), _KS("@vh|")) != 0) {
			path << "vh@" << vh_name << "/";
		}
		path << access->get_qname() << "/table@" << getUrlValue("table_name") << "/chain#" << getUrlValue("id");
		if (mark) {
			path << "/mark";
		} else {
			path << "/acl";
		}
		kconfig::remove(file.str(), path.str().str(), (uint32_t)urlValue.attribute.get_int("model"));
		KStringBuf url;
		url << "/editchainform?access_type=" << getUrlValue("access_type")
			<< "&table_name=" << getUrlValue("table_name")
			<< "&file=" << getUrlValue("file")
			<< "&index=" << getUrlValue("index")
			<< "&id=" << getUrlValue("id")
			<< "&vh=" << getUrlValue("vh");
		return sendRedirect(url.c_str());

	}
	if (strcmp(rq->sink->data.url->path, "/editchainform") == 0) {
		//sendHeader(200);
		KStringBuf s;
		KStringBuf url;
		if (vh) {
			conf.gvm->getMenuHtml(s, vh.get(), url);
		}
		access->add_chain_form(s, vh_name.c_str(), getUrlValue("table_name"), getUrlValue("file"), urlValue.attribute.get_int("index"), atoi(getUrlValue("id").c_str()), (getUrlValue("add") == "1" ? true : false));
		s << endTag();
		return sendHttp(s.str());
	}

	hit = false;
	return false;
}
bool KHttpManage::start_vhs(bool& hit) {
	KString err_msg;
	KStringBuf s;
	if (strcmp(rq->sink->data.url->path, "/vhslist") == 0) {
		hit = true;
		conf.gvm->getHtml(s, "", 0, urlValue);
		return sendHttp(s.str());
		//return sendHttp(conf.gvm->getVhsList());
	}
	if (strcmp(rq->sink->data.url->path, "/vhlist") == 0) {
		hit = true;
		conf.gvm->getHtml(s, getUrlValue("name"), atoi(getUrlValue(
			"id").c_str()), urlValue);
		return sendHttp(s.str());
	}
	if (strcmp(rq->sink->data.url->path, "/vhbase") == 0) {
		hit = true;
		KStringBuf url;
		url << "/vhlist?id=" << removeUrlValue("id");
		if (getUrlValue("action") != "vh_edit" && getUrlValue("action") != "vh_add") {
			url << "&name=" << getUrlValue("name");
		}
		conf.gvm->vh_base_action(urlValue, err_msg);
		if (err_msg.size() > 0) {
			return sendErrPage(err_msg.c_str());
		}
		return sendRedirect(url.str().c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/reload_vh") == 0) {
		do_config(false);
		hit = true;
		s << klang["reload_vh_msg"] << "<br>";
		conf.gvm->getHtml(s, getUrlValue("name"), atoi(getUrlValue("id").c_str()), urlValue);
		return sendHttp(s.str());
	}
	return true;
}
bool KHttpManage::sendProcessInfo() {

	KStringBuf s;
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
KGL_RESULT KHttpManage::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	this->rq = rq;
	this->out = out;
	return handle_request(handler, this, rq, in, out);
}
bool KHttpManage::start(bool& hit) {
	parseUrl(rq->sink->data.url->param);
	parsePostData();
	if (strcmp(rq->sink->data.url->path, "/command") == 0) {
		return runCommand();
	}
	if (postData) {
		urlValue.parse(postData);
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
	}
	if (strcmp(rq->sink->data.url->path, "/apilist") == 0) {
		return extends(2);
	}
	if (strcmp(rq->sink->data.url->path, "/cgilist") == 0) {
		return extends(3);
	}
	if (strcmp(rq->sink->data.url->path, "/macserver") == 0) {
		return extends(1);
	}
	if (strcmp(rq->sink->data.url->path, "/extends") == 0) {
		return extends();
	}
#ifdef ENABLE_MULTI_SERVER
	if (strcmp(rq->sink->data.url->path, "/macserveradd") == 0) {
		KString err_msg;
		bool edit = getUrlValue("action") == "edit";
		if (conf.gam->new_server(edit, urlValue.attribute, err_msg)) {
			return sendRedirect("/macserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/del_macserver") == 0) {
		KString err_msg;
		if (conf.gam->remove_server(getUrlValue("name"), err_msg)) {
			return sendRedirect("/macserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/macserver_node_form") == 0) {
		KStringBuf s;
		conf.gam->macserver_node_form(s, getUrlValue("name"), getUrlValue("action"), atoi(getUrlValue("id").c_str()));
		return sendHttp(s.str());
	}
	if (strcmp(rq->sink->data.url->path, "/macserver_node") == 0) {
		KString err_msg;
		if (conf.gam->macserver_node(urlValue.attribute, err_msg)) {
			return sendRedirect("/macserver");
		}
		return sendErrPage(err_msg.c_str());
	}
#endif
	if (strcmp(rq->sink->data.url->path, "/acserveradd") == 0) {
		KString err_msg;
		if (conf.gam->new_server(false, urlValue.attribute, err_msg)) {
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/acserveredit") == 0) {
		KString err_msg;
		if (conf.gam->new_server(true, urlValue.attribute, err_msg)) {
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/acserverdelete") == 0) {
		KString err_msg;
		if (conf.gam->remove_server(getUrlValue("name"), err_msg)) {
			return sendRedirect("/acserver");
		}
		return sendErrPage(err_msg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/reload_config") == 0) {
		do_config(false);
		wait_load_config_done();
		return sendMainFrame();
	}
	if (strcmp(rq->sink->data.url->path, "/connect_per_ip") == 0) {
		return sendHttp(get_connect_per_ip().c_str());
	}

	if (strcasecmp(rq->sink->data.url->path, "/connection") == 0) {
		KStringBuf s;
		KConnectionInfoContext ctx;
		ctx.total_count = 0;
		ctx.debug = atoi(getUrlValue("debug").c_str());
		ctx.vh = NULL;
		ctx.translate = true;
		kgl_iterator_sink(kgl_connection_iterator, (void*)&ctx);

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
		s << ctx.s.str();
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
		return sendHttp(s.str());
	}

#ifdef ENABLE_VH_RUN_AS
	if (strcmp(rq->sink->data.url->path, "/cmdform") == 0) {
		KString errMsg;
		if (conf.gam->cmdForm(urlValue.attribute, errMsg)) {
			return sendRedirect("/extends?item=3");
		}
		return sendErrPage(errMsg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/delcmd") == 0) {
		KString errMsg;
		if (conf.gam->delCmd(getUrlValue("name"), errMsg)) {
			return extends(3);
		}
		return sendErrPage(errMsg.c_str());

	}
#endif
	if (strcmp(rq->sink->data.url->path, "/apiform") == 0) {
		KString errMsg;
		if (conf.gam->apiForm(urlValue.attribute, errMsg)) {
			return sendRedirect("/extends?item=2");
		}
		return sendErrPage(errMsg.c_str());
	}
	if (strcmp(rq->sink->data.url->path, "/delapi") == 0) {
		KString errMsg;
		if (conf.gam->delApi(getUrlValue("name"), errMsg)) {
			return extends(2);
		}
		return sendErrPage(errMsg.c_str());

	}
	if (strcmp(rq->sink->data.url->path, "/changelang") == 0) {
		auto lang = getUrlValue("lang");
		kconfig::update("lang"_CS, 0, &lang, nullptr, kconfig::EvUpdate | kconfig::FlagCreate);
		return sendRedirect("/");
	}
	if (strcmp(rq->sink->data.url->path, "/process") == 0) {
		return sendProcessInfo();
	}
	if (strcmp(rq->sink->data.url->path, "/process_kill") == 0) {
		killProcess(getUrlValue("name"), getUrlValue("user"), atoi(getUrlValue("pid").c_str()));
		return sendRedirect("/process");
	}
	if (strcmp(rq->sink->data.url->path, "/reboot") == 0) {
		return reboot();
	}
	if (strcmp(rq->sink->data.url->path, "/reboot_submit") == 0) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		console_call_reboot();
		return sendHttp("");
	}
	if (strcmp(rq->sink->data.url->path, "/clean_all_cache") == 0) {
		dead_all_obj();
		return sendMainFrame();
	}
#ifdef ENABLE_DISK_CACHE
	if (strcmp(rq->sink->data.url->path, "/scan_disk_cache.km") == 0) {
		rescan_disk_cache();
		return sendMainFrame();
	}
	if (strcmp(rq->sink->data.url->path, "/flush_disk_cache.km") == 0) {
		cache.flush_mem_to_disk();
		saveCacheIndex();
		return sendMainFrame();
	}
	void create_cache_dir(const char* disk_dir);
	if (strcmp(rq->sink->data.url->path, "/format_disk_cache_dir.km") == 0) {
		auto dir = getUrlValue("dir");
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
	if (hit) {
		return result;
	}
	return start_listen(hit);
}

void init_manager_handler() {
	KHttpManage::handler.add(_KS("/*"), [](kgl_str_t* path, void* data, KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) -> KGL_RESULT {
		KHttpManage* hm = (KHttpManage*)data;
		if (path->len == 1 && *path->data == '/') {
			hm->sendMainPage();
			return KGL_OK;
		}
		bool hit = true;
		if (!hm->start(hit)) {
			KBIT_SET(hm->rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		}
		if (!hit) {
			return send_error2(hm->rq, STATUS_NOT_FOUND, "no such file");
		}
		return KGL_OK;
		});
	KHttpManage::handler.add(_KS("/reload"), [](kgl_str_t* path, void* data, KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) -> KGL_RESULT {
		KUrlValue url;
		if (rq->sink->data.url->param) {
			url.parse((const char*)rq->sink->data.url->param);
		}
		auto &&file = url.attribute["file"];
		if (file == KString::empty_string) {
			kconfig::reload();
			return send_http2(rq, nullptr, STATUS_NO_CONTENT, nullptr);
		}
		auto name = kstring_from2(file.c_str(), file.size());
		defer(kstring_release(name));
		auto result = kconfig::reload_config(name, true);
		if (result) {
			return send_http2(rq, nullptr, STATUS_NO_CONTENT, nullptr);
		} else {
			return send_http2(rq, nullptr, STATUS_NOT_FOUND, nullptr);
		}
		});
	KHttpManage::handler.add(_KS("/cfg/*"), [](kgl_str_t* path, void* data, KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) -> KGL_RESULT {
		if (path->len == 0) {
			return response_redirect(out, _KS("/cfg/"), 308);
		}
		if (rq->sink->data.meth == METH_GET) {
			auto locker = kconfig::lock();
			const char* orig_path = path->data;
			size_t orig_len = path->len;
			auto tree = kconfig::find((const char**)&path->data, &path->len);
			if (!tree) {
				return out->f->error(out->ctx, 404, _KS("not found"));
			}
			out->f->write_header(out->ctx, kgl_header_content_type, _KS("text/xml"));
			KBIT_SET(rq->ctx.obj->index.flags, ANSW_LOCAL_SERVER);
			out->f->write_unknow_header(out->ctx, _KS("x-gzip"), _KS("1"));
			KBodyWStream st;
			auto result = out->f->write_header_finish(out->ctx, -1, &st.body);
			if (result != KGL_OK) {
				return result;
			}
			st.write_all(_KS("<result path='"));
			if (*orig_path != '/') {
				st.write_all(_KS("/"));
			}
			if (orig_len > path->len) {
				st.write_all(orig_path, orig_len - path->len);
			}
			st.write_all(_KS("' listen='"));
			if (tree->ls) {
				st << (int)tree->ls->config_flag();
			}
			st.write_all(_KS("'>"));
			auto node = tree->node;
			while (node) {
				st.write_all(_KS("<xml file='"));
				auto name = node->file->get_name();
				st.write_all(name->data, name->len);
				st.write_all(_KS("'"));
				st.write_all(_KS(">"));
				node->xml->write(&st);
				st.write_all(_KS("</xml>"));
				if (!tree->is_merge()) {
					break;
				}
				node = node->next;
			}
			if (!tree->child.empty()) {
				st.write_all(_KS("<child>"));
				for (auto it = tree->child.first(); it; it = it->next()) {
					auto v = it->value();
					if (v->key.tag->len > 0) {
						st.write_all(_KS("<"));
						st.write_all(v->key.tag->data, v->key.tag->len);
						if (v->key.vary && v->key.vary->len > 0) {
							st.write_all(_KS(" vary='"));
							st.write_all(v->key.vary->data, v->key.vary->len);
							st.write_all(_KS("'"));
						}
						st.write_all(_KS("/>"));
					} else if (v->key.vary && v->key.vary->len > 0) {
						st.write_all(_KS("<"));
						st.write_all(v->key.vary->data, v->key.vary->len);
						st.write_all(_KS(" is_vary='1'/>"));
					}
				}
				st.write_all(_KS("</child>"));
			}
			return st.write_end(st.write_all(_KS("</result>")));
		}
		return KGL_OK;
		});
}
