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
#ifndef KVIRTUALHOST_H
#define KVIRTUALHOST_H
 /**
  * virtual host
  */
#include <time.h>
#include <string>
#include <map>
#include "global.h"
#include "KLogElement.h"
  //#include "KLocalFetchObject.h"
#include "KFileName.h"
#include "KCountable.h"
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KPathRedirect.h"
#include "KAccess.h"
#include "utils.h"
#include "KBaseVirtualHost.h"
#include "KRequestQueue.h"
#include "extern.h"
#include "KFlowInfo.h"
#include "KConnectionLimit.h"
#include "KXmlAttribute.h"
//由vh的引用，计算连接数的差异
#define VH_REFS_CONNECT_DELTA 2
inline KString get_vh_full_doc_root(const KString &doc_root) {
	KString full_path;
	if (!kgl_is_absolute_path(doc_root.c_str())) {
		full_path = conf.path + doc_root;
	} else {
#ifdef _WIN32
		if (doc_root[0] == '/') {
			full_path = conf.diskName + doc_root;
		} else
#endif
		full_path = doc_root;
	}
	pathEnd(full_path);
	return full_path;
}
/**
* 虚拟主机类
*/
class KVirtualHost final : public KBaseVirtualHost, public KSslConfig
{
public:
	KVirtualHost(const KString& name);
	bool setDocRoot(const KString& docRoot);
	KSubVirtualHost* getFirstSubVirtualHost() {
		if (hosts.size() == 0) {
			return NULL;
		}
		return *(hosts.begin());
	}
	bool is_global() override {
		return false;
	}
	bool parse_xml(const khttpd::KXmlNodeBody* body, KVirtualHost* ov);
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir;
	}
	friend class KHttpRequest;
	friend class KNsVirtualHost;
	friend class KVirtualHostManage;
	union
	{
		struct
		{
			uint16_t closed : 1;
			uint16_t browse : 1;
			//统计流量
			uint16_t fflow : 1;
			uint16_t ip_hash : 1;
			uint16_t inherit : 1;
#ifdef ENABLE_VH_RUN_AS
			uint16_t need_kill_process : 1;
#ifdef _WIN32
			uint16_t logoned : 1;
			uint16_t logonresult : 1;
#endif
#endif
#ifndef _WIN32
			uint16_t chroot : 1;
#endif
			uint16_t app_share : 1;
			/* 应用程序池数量 */
			uint8_t app;
		};
		uint32_t flags;
	};
	volatile uint32_t ref;
	KVirtualHost* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
#ifdef ENABLE_USER_ACCESS
	int checkRequest(KHttpRequest* rq, KSafeSource& fo) {
		if (!access[REQUEST]) {
			return JUMP_ALLOW;
		}
		return access[REQUEST]->check(rq, NULL, fo);
	}
	int checkResponse(KHttpRequest* rq, KSafeSource& fo) {
		if (!access[RESPONSE]) {
			return JUMP_ALLOW;
		}
		return access[RESPONSE]->check(rq, rq->ctx.obj, fo);
	}
	int checkPostMap(KHttpRequest* rq, KSafeSource& fo) {
		if (!access[RESPONSE]) {
			return JUMP_ALLOW;
		}
		return access[RESPONSE]->check_post_map(rq, rq->ctx.obj, fo);
	}
	KString user_access;
#endif
	KString doc_root;
	KString GetDocumentRoot() {
		KString orig_doc_root;
		if (strncasecmp(doc_root.c_str(), conf.path.c_str(), conf.path.size()) == 0) {
			orig_doc_root = doc_root.substr(conf.path.size());
		} else {
			orig_doc_root = doc_root;
		}
		strip_path_end(orig_doc_root);
		return orig_doc_root;
	}
#ifdef ENABLE_VH_LOG_FILE
	KLogElement* logger;
	KString logFile;
	void parse_log_config(const KXmlAttribute& attr);
#endif
#ifdef ENABLE_VH_RUN_AS
	/*
	 * 计算是否需要杀掉对应的进程,返回true则要杀掉进程，否则不杀掉进程
	 */
	bool caculateNeedKillProcess(KVirtualHost* ov);
	void KillAllProcess();
#endif
#ifdef ENABLE_VH_QUEUE
	void initQueue(unsigned max_worker, unsigned max_queue, KVirtualHost* ov) {
		if (queue) {
			return;
		}
		if (max_worker > 0) {
			if (ov) {
				queue = ov->queue;
			}
			if (queue) {
				queue->addRef();
			} else {
				queue = new KRequestQueue;
			}
			queue->set(max_worker, max_queue);
		}
	}
	unsigned getWorkerCount();
	unsigned getQueueSize();
	KRequestQueue* queue;
#endif
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit* refsSpeedLimit() {
		KSpeedLimit* sl = NULL;
		lock.Lock();
		sl = this->sl;
		if (sl) {
			sl->addRef();
		}
		lock.Unlock();
		return sl;
	}
	void SetStatus(int status) {
		closed = (status != 0);
	}
	void setSpeedLimit(const char* speed_limit_str, KVirtualHost* ov);
	void setSpeedLimit(int speed_limit, KVirtualHost* ov);
	int GetConnectionCount();
	bool addConnection(KHttpRequest* rq) {
		if (cur_connect == NULL || max_connect == 0) {
			return true;
		}
		return cur_connect->addConnection(rq, max_connect);
	}
	void initConnect(KVirtualHost* ov) {
		if (cur_connect) {
			return;
		}
		if (max_connect > 0) {
			if (ov) {
				cur_connect = ov->cur_connect;
			}
			if (cur_connect) {
				cur_connect->addRef();
			} else {
				cur_connect = new KConnectionLimit;
			}
		}
	}
	//当前连接数信息
	KConnectionLimit* cur_connect;
	//连接数限制
	int max_connect;
	//带宽限制
	int speed_limit;
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo* refsFlowInfo() {
		KFlowInfo* flow = NULL;
		lock.Lock();
		flow = this->flow;
		if (flow) {
			flow->addRef();
		}
		lock.Unlock();
		return flow;
	}
	void setFlow(bool fflow, KVirtualHost* ov) {
		lock.Lock();
		if (flow) {
			flow->release();
			flow = NULL;
		}
		this->fflow = fflow;
		if (fflow) {
			if (ov) {
				flow = ov->flow;
			}
			if (flow) {
				flow->addRef();
			} else {
				flow = new KFlowInfo;
			}
		}
		lock.Unlock();
	}
	INT64 get_speed(bool reset) {
		if (flow) {
			return flow->get_speed(reset);
		}
		return 0;
	}

#endif
	bool alias(bool internal, const char* path, KFileName* file, bool& exsit, int flag);
	char* alias(bool internal, const char* path);
	//用于webdav等应用校验
	KBaseRedirect* refsPathRedirect(const char* path, int path_len);
	KFetchObject* findPathRedirect(KHttpRequest* rq, KFileName* file, const char* path,
		bool fileExsit, bool& result);
	/*
	 check if a file will map to the rd
	 */
	bool isPathRedirect(KHttpRequest* rq, KFileName* file, bool fileExsit, KRedirect* rd);
	KFetchObject* findFileExtRedirect(KHttpRequest* rq, KFileName* file, bool fileExsit, bool& result);
	//KFetchObject *findDefaultRedirect(KHttpRequest *rq,KFileName *file,bool fileExsit);
	KString name;

#ifdef ENABLE_BASED_PORT_VH
	std::list<KString> binds;
	void get_bind_html(const KString& url, KWStream& s);
#endif
	bool empty() {
		if (!hosts.empty()) {
			return false;
		}
		return true;
	}
	std::list<KSubVirtualHost*> hosts;
	void get_host_html(const KString& url, KWStream& s);
#ifdef ENABLE_VH_RUN_AS
	//KString add_dir;
	bool setRunAs(const KString& user, const KString& group);

#ifndef _WIN32
	int id[2];
#endif
	/*
	 * run user
	 */
	KString user;
	/*
	 * run group
	 */
	KString group;
	KString getUser() {
		return user;
	}
#ifdef _WIN32
	HANDLE logon(bool& result);
	Token_t getLogonedToken(bool& result) {
		result = logoned;
		return token;
	}
#endif
	Token_t createToken(bool& result);
	Token_t getProcessToken(bool& result);
	static void createToken(Token_t token);
#else
	/*
	对于不支持虚拟主机运行用户时返回一个全局用户名
	*/
	KString getUser() {
		return "-";
	}
#endif
	std::vector<KString> apps;
	void setApp(int app);
	KString getApp(KHttpRequest* rq);
	static void closeToken(Token_t token);
	bool loadApiRedirect(KApiPipeStream* st, int workType);
	void setAccess(const KString& access_file);
	KString htaccess;
	KSafeAccess access[2];
	bool has_user_access() {
		if (user_access && user_access[0] != '-') {
			return true;
		}
		return false;
	}
	void get_access_file(KWStream& s) {
		s << "@vh|" << name << "_access";
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	kgl_ssl_ctx* ssl_ctx;
	bool setSSLInfo(const KString& certfile, const KString& keyfile, const KString& cipher, const KString& protocols);
	KString get_cert_file() const override;
	KString get_key_file() const override;
	kgl_ssl_ctx* refs_ssl_ctx() override {
		if (ssl_ctx == nullptr) {
			this->ssl_ctx = KSslConfig::refs_ssl_ctx();
		}
		if (ssl_ctx == nullptr) {
			return nullptr;
		}
		kgl_add_ref_ssl_ctx(ssl_ctx);
		return ssl_ctx;
	}
#endif
private:
	~KVirtualHost();
#ifdef ENABLE_VH_RS_LIMIT
	//当前带宽信息
	KSpeedLimit* sl;
#endif
#ifdef ENABLE_VH_FLOW
	//流量表
	KFlowInfo* flow;
#endif
	bool loadApiRedirect(KRedirect* rd, KApiPipeStream* st, int workType);
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	HANDLE token;
#endif
#endif
	void copy_to(KVirtualHost* vh);
	KSubVirtualHost* parse_host(const khttpd::KXmlNodeBody* body) {
		KSubVirtualHost* svh = new KSubVirtualHost(this);
		svh->set_doc_root(this->doc_root.c_str(), body->attributes("dir", nullptr));
#ifdef ENABLE_SVH_SSL
		auto crt = body->attributes["certificate"];
		auto key = body->attributes["certificate_key"];
		if (!crt.empty()) {
			svh->set_ssl_info(crt.c_str(), key.c_str(), true);
		}
#endif
		svh->setHost(body->get_text_cstr(""));
		return svh;
	}
public:
#ifdef ENABLE_USER_ACCESS
	void access_config_listen(kconfig::KConfigTree* tree, KVirtualHost* ov);
#endif
};
using KSafeVirtualHost = KSharedObj<KVirtualHost>;
#endif /*KVIRTUALHOST_H_*/
