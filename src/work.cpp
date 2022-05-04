/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */

#include "global.h"

#include<time.h>

#include <sstream>

#include<map>
#include<string>
#include<list>
#include<stdarg.h>

 //#include	"modules.h"
#include	"utils.h"
#include	"http.h"
#include	"cache.h"
#include	"log.h"
#include 	"KHttpManage.h"
#include 	"kselector_manager.h"
#include 	"lib.h"
#include	"kmalloc.h"
#include "KHttpObjectHash.h"
#include "kthread.h"
#include "KVirtualHostManage.h"
#include "KSequence.h"
#include "kmd5.h"
#include "KFastcgiUtils.h"
#include "KFastcgiFetchObject.h"
#include "KApiFetchObject.h"
#include "KObjectList.h"
#include "KHttpProxyFetchObject.h"
#include "KPoolableSocketContainer.h"
#include "lang.h"
#include "time_utils.h"
#include "KHttpDigestAuth.h"
#include "KHttpBasicAuth.h"
#include "KSSLSniContext.h"
#include "KBigObjectContext.h"
#include "KAcserverManager.h"
#include "KHttpUpstream.h"
#ifndef KANGLE_FREE
#include "KWhiteList.h"
#endif
#include "KHttp2.h"
#include "HttpFiber.h"
#include "KHttpFilterManage.h"
#include "ksapi.h"
#include "kaddr.h"
#include "KLogDrill.h"
#include "kfiber.h"

volatile uint32_t kgl_log_count = 0;
using namespace std;
unsigned total_connect = 0;
namespace kangle {
	typedef std::map<ip_addr, unsigned> intmap;
	intmap m_ip;
	KMutex ip_lock;

	std::string get_connect_per_ip() {
		std::stringstream s;
		s << "<html><head><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
		s << LANG_MAX_CONNECT_PER_IP << conf.max_per_ip;
		s << "<script language='javascript'>\n\
	var ips=new Array();\n\
	function show_ips(){\n\
		document.write('<table border=1><tr><td>" << LANG_IP << "</td><td>" << LANG_CONNECT_COUNT << "</td></tr>');\n\
		for(var i=0;i<ips.length;i++){\n\
			document.write('<tr><td>'+ips[i][0]+'</td><td>'+ips[i][1]+'</td>');\n\
		}\
		document.write('</table>');\n\
	}\n\
	function compare_ips(a,b) {\n\
		return b[1] - a[1];\n\
	}\n";
		ip_lock.Lock();
		intmap::iterator it;
		for (it = m_ip.begin(); it != m_ip.end(); it++) {
			char ip[MAXIPLEN];
			ksocket_ipaddr_ip(&(*it).first, ip, sizeof(ip));
			s << "ips.push(new Array('" << ip << "'," << (*it).second << "));\n";
		}
		ip_lock.Unlock();
		s << "ips.sort(compare_ips);\n\
	show_ips();\n\
	</script>";
		//s << "<!-- total connect = " << total << " -->\n";
		s << endTag() << "</body></html>";
		return s.str();
	}
	void del_request(void* data)
	{
		ip_lock.Lock();
		total_connect--;
#ifdef RQ_LEAK_DEBUG
		klist_remove(&c->queue_edge);
#endif
		ip_lock.Unlock();
	}
	void del_request_per_ip(void* data)
	{
		kconnection* c = (kconnection*)data;
		ip_addr ip;
		ksocket_ipaddr(&c->addr, &ip);
		intmap::iterator it2;
		ip_lock.Lock();
		total_connect--;
#ifdef RQ_LEAK_DEBUG
		klist_remove(&c->queue_edge);
#endif
		it2 = m_ip.find(ip);
		assert(it2 != m_ip.end());
		(*it2).second--;
		if ((*it2).second == 0) {
			m_ip.erase(it2);
		}
		ip_lock.Unlock();
	}
	bool is_upstream_tcp(Proto_t proto)
	{
		switch (proto) {
		case Proto_http:
			return false;
		default:
			return true;
		}
	}
	kgl_connection_result kgl_on_new_connection(kconnection* c)
	{
		unsigned max = (unsigned)conf.max;
		unsigned max_per_ip = conf.max_per_ip;
		kassert(c->server);
		if (KBIT_TEST(c->server->flags, WORK_MODEL_MANAGE)) {
			if (max > 0) {
				max += 100;
			}
			if (max_per_ip > 0) {
				max_per_ip += 10;
			}
		}
		kgl_pool_cleanup_t* pt;
		intmap::iterator it2;
		ip_addr ip;
		ip_lock.Lock();
		if (max > 0 && total_connect >= max) {
			//printf("total_connect=%d,max=%d\n",total_connect,max);
			ip_lock.Unlock();
			return kgl_connection_too_many;
		}
		if (conf.keep_alive_count > 0 && total_connect >= conf.keep_alive_count) {
			//SET(c->st_flags, STF_NO_KA);
		}
		if (max_per_ip == 0) {
			total_connect++;
#ifdef RQ_LEAK_DEBUG
			klist_append(&all_connection, &c->queue_edge);
#endif
			ip_lock.Unlock();
			pt = kgl_pool_cleanup_add(c->pool, 0);
			pt->handler = del_request;
			pt->data = c;
			return kgl_connection_success;
		}
		ksocket_ipaddr(&c->addr, &ip);
		it2 = m_ip.find(ip);
		if (it2 == m_ip.end()) {
			m_ip.insert(std::pair<ip_addr, unsigned>(ip, 1));
		} else {
			if ((*it2).second >= max_per_ip) {
				goto max_per_ip;
			}
			(*it2).second++;
		}
		total_connect++;
#ifdef RQ_LEAK_DEBUG
		klist_append(&all_connection, &c->queue_edge);
#endif
		ip_lock.Unlock();
		pt = kgl_pool_cleanup_add(c->pool, 0);
		pt->handler = del_request_per_ip;
		pt->data = c;
		return kgl_connection_success;
	max_per_ip:
		ip_lock.Unlock();
#ifdef ENABLE_BLACK_LIST
		if (conf.per_ip_deny) {
			char ips[MAXIPLEN];
			ksocket_sockaddr_ip(&c->addr, ips, MAXIPLEN);
			conf.gvm->globalVh.blackList->AddDynamic(ips);
		}
#endif
		return kgl_connection_per_limit;
	}
};
void log_access(KHttpRequest* rq) {
	//printf("log_access=[%p]\n", rq);
	int log_radio = conf.log_radio;
	if (log_radio > 1) {
		//ÈÕÖ¾³éÑù
		uint32_t log_count = katom_inc((void*)&kgl_log_count);
		if (log_count % log_radio != 0) {
			return;
		}
	}
	if (rq->isBad()) {
		klog(KLOG_ERR, "BAD REQUEST FROM [%s].\n", rq->getClientIp());
		return;
	}
	INT64 sended_length = rq->sink->data.send_size;
	KStringBuf l(512);
	KLogElement* s = &accessLogger;
	auto svh = rq->get_virtual_host();
#ifndef HTTP_PROXY
	if (svh) {
#ifdef ENABLE_BLACK_LIST
		KIpList* iplist = svh->vh->blackList;
		if (iplist) {
			iplist->addStat(rq->sink->data.flags, rq->filter_flags);
		}
#endif
#ifdef ENABLE_VH_LOG_FILE
		if (svh->vh->logger) {
			s = svh->vh->logger;
		}
#endif
	}
#endif
	if (s->place == LOG_NONE
#ifdef ENABLE_LOG_DRILL
		&& conf.log_drill <= 0
#endif
		) {
		return;
	}
	char* referer = NULL;
	const char* user_agent = NULL;
	char tmp[64];
	int default_port = 80;
	if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
		default_port = 443;
	}
	l << rq->getClientIp();
	//l.WSTR(":");
	//l << rq->sink->get_remote_port();
	l.WSTR(" - ");
	if (rq->auth) {
		const char* user = rq->auth->getUser();
		if (user && *user) {
			l << user;
		} else {
			l.WSTR("-");
		}
	} else {
		l.WSTR("-");
	}
	l.WSTR(" ");
	timeLock.Lock();
	l.write_all((char*)cachedLogTime, 28);
	timeLock.Unlock();
	l.WSTR(" \"");
	l << rq->getMethod();
	l.WSTR(" ");
#ifdef HTTP_PROXY
	if (rq->sink->data.meth != METH_CONNECT)
#endif
		l << (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL) ? "https://" : "http://");
	KUrl* url = &rq->sink->data.raw_url;
	referer = (char*)rq->GetHttpValue("Referer");
	user_agent = rq->GetHttpValue("User-Agent");
	l << url->host;
	if (url->port != default_port) {
		l << ":" << url->port;
	}
#ifdef HTTP_PROXY
	if (rq->sink->data.meth != METH_CONNECT)
#endif
		l << url->path;
	if (url->param) {
		l << "?" << url->param;
	}
#ifdef ENABLE_HTTP2
	if (rq->sink->data.http_major > 1) {
		l.WSTR(" HTTP/2\" ");
	} else
#endif	
		l.WSTR(" HTTP/1.1\" ");
	l << rq->sink->data.status_code << " ";
#ifdef _WIN32
	const char* formatString = "%I64d";
#else
	const char* formatString = "%lld";
#endif
	sprintf(tmp, formatString, sended_length);
	l << tmp;
#ifndef NDEBUG
	const char* range = rq->GetHttpValue("Range");
	if (range) {
		l << " \"" << range << "\"";
	}
#endif
	//s->log(formatString,rq->send_ctx.send_size);
	l.WSTR(" \"");
	if (referer) {
		l << referer;
	} else {
		l.WSTR("-");
	}
	l.WSTR("\" \"");
	if (user_agent) {
		l << user_agent;
	} else {
		l.WSTR("-");
	}
	//*
	l.WSTR("\"[");
#ifndef NDEBUG
	l << "F" << (unsigned)rq->sink->data.flags << "f" << (unsigned)rq->filter_flags;
	//l << "u" << (int)rq->ctx->upstream_socket;
	if (!rq->ctx->upstream_expected_done) {
		l.WSTR("d");
	}
#endif
#ifdef KSOCKET_SSL
#ifdef SSL_READ_EARLY_DATA_SUCCESS
	kssl_session* ssl = rq->sink->GetSSL();
	if (ssl && ssl->in_early) {
		l.WSTR("e");
	}
#endif
#endif
	if (rq->ctx->read_huped) {
		l.WSTR("h");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_CACHE_HIT)) {
		l.WSTR("C");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_OBJ_STORED)) {
		l.WSTR("S");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_UPSTREAM)) {
		l.WSTR("U");
		if (KBIT_TEST(rq->sink->data.flags, RQ_UPSTREAM_ERROR)) {
			l.WSTR("E");
		}
	}
	if (rq->ctx->parent_signed) {
		l.WSTR("P");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_OBJ_VERIFIED)) {
		l.WSTR("V");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_TE_COMPRESS)) {
		l.WSTR("Z");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_TE_CHUNKED)) {
		l.WSTR("K");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_KEEP_CONNECTION | RQ_CONNECTION_CLOSE) == RQ_HAS_KEEP_CONNECTION) {
		l.WSTR("L");
	}
	if (rq->ctx->upstream_connection_keep_alive) {
		l.WSTR("l");
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_INPUT_CHUNKED)) {
		l.WSTR("I");
	}
	//{{ent
	if (KBIT_TEST(rq->filter_flags, RQF_CC_PASS)) {
		l.WSTR("p");
	}
	if (KBIT_TEST(rq->filter_flags, RQF_CC_HIT)) {
		l.WSTR("c");
	}
	//}}
	if (rq->sink->data.first_response_time_msec > 0) {
		l.WSTR("T");
		INT64 t2 = rq->sink->data.first_response_time_msec - rq->sink->data.begin_time_msec;
		l << t2;
	}
	if (rq->sink->data.mark != 0) {
		l.WSTR("m");
		l << (int)rq->sink->data.mark;
	}
	//l.WSTR("a");
	//l << rq->ctx->us_code;
	l.WSTR("]");
	if (conf.log_event_id) {
		l.WSTR(" ");
		l.add(rq->sink->data.begin_time_msec, INT64_FORMAT_HEX);
		l.WSTR("-");
		l.add((INT64)rq, INT64_FORMAT_HEX);
	}
#if 0
	l.WSTR(" ");
	l.addHex(rq->sink->data.if_modified_since);
	l.WSTR(" ");
	l.addHex(rq->sink->data.if_modified_since);
	l.WSTR(" ");
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_NONE_MATCH)) {
		l.WSTR("inm");
	}
	if (rq->sink->data.if_none_match) {
		l.write_all(rq->sink->data.if_none_match->data, rq->sink->data.if_none_match->len);
	}
#endif
	l.WSTR("\n");
	//*/
	if (s->place != LOG_NONE) {
		s->startLog();
		s->write(l.getString(), l.getSize());
		s->endLog(false);
	}
#ifdef ENABLE_LOG_DRILL
	if (conf.log_drill > 0) {
		add_log_drill(rq, l);
	}
#endif
}

KGL_RESULT stageHttpManageLogin(KHttpRequest* rq)
{
	if (rq->auth) {
		delete rq->auth;
		rq->auth = NULL;
	}
#ifdef ENABLE_DIGEST_AUTH
	if (conf.auth_type == AUTH_DIGEST) {
		KHttpDigestAuth* auth = new KHttpDigestAuth();
		auth->init(rq, PROGRAM_NAME);
		rq->auth = auth;
	} else
#endif
		rq->auth = new KHttpBasicAuth(PROGRAM_NAME);
	rq->auth->set_auth_header(AUTH_HEADER_WEB);
	const char path_split_str[2] = { PATH_SPLIT_CHAR,0 };
	KAutoBuffer buffer(rq->sink->pool);
	buffer << "<html><body>Please set the admin user and password in the file: <font color='red'>"
		<< "kangle_installed_path" << path_split_str << "etc" << path_split_str
		<< "config.xml</font> like this:"
		"<font color=red><pre>"
		"&lt;admin user='admin' password='kangle' admin_ips='127.0.0.1|*'/&gt;"
		"</pre></font>\n"
		"The default admin user is admin, password is kangle</body></html>";
	return send_auth2(rq, &buffer);
}
KGL_RESULT stageHttpManage(KHttpRequest* rq)
{

	conf.admin_lock.Lock();
	if (!checkManageLogin(rq)) {
		conf.admin_lock.Unlock();
		char ips[MAXIPLEN];
		rq->sink->get_peer_ip(ips, sizeof(ips));
		klog(KLOG_WARNING, "[ADMIN_FAILED] %s %s\n", ips, rq->sink->data.raw_url.path);
		return stageHttpManageLogin(rq);
	}
	conf.admin_lock.Unlock();
	char ips[MAXIPLEN];
	rq->sink->get_peer_ip(ips, sizeof(ips));
	klog(KLOG_NOTICE, "[ADMIN_SUCCESS]%s %s%s%s\n",
		ips, rq->sink->data.raw_url.path,
		(rq->sink->data.raw_url.param ? "?" : ""), (rq->sink->data.raw_url.param ? rq->sink->data.raw_url.param : ""));
	if (strstr(rq->sink->data.url->path, ".whm")
		|| strcmp(rq->sink->data.url->path, "/logo.gif") == 0
		|| strcmp(rq->sink->data.url->path, "/main.css") == 0) {
		KBIT_CLR(rq->sink->data.flags, RQ_HAS_AUTHORIZATION);
		assert(rq->IsFetchObjectEmpty());
		auto svh = conf.sysHost->getFirstSubVirtualHost();
		rq->sink->data.bind_opaque(svh);
		if (svh) {
			svh->vh->addRef();
			return fiber_http_start(rq);
		}
		assert(false);
	}
	rq->AppendFetchObject(new KHttpManage);
	return process_request(rq, rq->GetFetchObject());
}
