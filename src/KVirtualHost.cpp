/*
 * KVirtualHost.cpp
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#include <vector>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "KVirtualHost.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KVirtualHostManage.h"
#include "KLogManage.h"
#include "KAccessParser.h"
#include "cache.h"
#include "utils.h"
#include "kmalloc.h"
#include "KApiRedirect.h"
#include "KApiPipeStream.h"
#include "extern.h"
#include "KAcserverManager.h"
#include "KDefer.h"
#include "KConfigTree.h"



KVirtualHost::KVirtualHost(const KString& name) {
	ref = 1;
	this->name = name;
	flags = 0;
#ifdef ENABLE_VH_LOG_FILE
	logger = NULL;
#endif
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	token = NULL;
#else
	id[0] = id[1] = 0;
#endif	
#endif
#ifdef ENABLE_VH_RS_LIMIT
	max_connect = 0;
	cur_connect = NULL;
	speed_limit = 0;
	sl = NULL;
#endif
#ifdef ENABLE_VH_FLOW
	flow = NULL;
#endif
#ifdef ENABLE_VH_QUEUE
	queue = NULL;
#endif
	inherit = true;
	app_share = 1;
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	ssl_ctx = NULL;
#endif
}
KVirtualHost::~KVirtualHost() {
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	if (token) {
		CloseHandle(token);
	}
#endif
#endif
#ifdef ENABLE_VH_LOG_FILE
	if (logger) {
		logManage.destroy(logger);
		//delete logger;
	}
#endif
	for (auto it2 = hosts.begin(); it2 != hosts.end(); it2++) {
		delete (*it2);
	}
#ifdef ENABLE_VH_RS_LIMIT
	if (sl) {
		sl->release();
	}
	if (cur_connect) {
		cur_connect->release();
	}
#endif
#ifdef ENABLE_VH_FLOW
	if (flow) {
		flow->release();
	}
#endif
#ifdef ENABLE_VH_QUEUE
	if (queue) {
		queue->release();
	}
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if (ssl_ctx) {
		kgl_release_ssl_ctx(ssl_ctx);
	}
#endif
}
bool KVirtualHost::isPathRedirect(KHttpRequest* rq, KFileName* file,
	bool fileExsit, KRedirect* rd) {
	bool result = false;
	int path_len = (int)strlen(rq->sink->data.url->path);
	lock.Lock();
	for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		if ((*it2)->match(rq->sink->data.url->path, path_len)
			&& (*it2)->allowMethod.matchMethod(rq->sink->data.meth)) {
			if (rd == (*it2)->rd) {
				result = true;
			}
			break;
		}
	}
	lock.Unlock();
	return result;
}
bool KVirtualHost::alias(bool internal, const char* path, KFileName* file, bool& exsit, int flag) {
	auto len = strlen(path);
	auto alias = find_alias(internal, path, len);
	if (!alias) {
		if (!inherit) {
			return false;
		}
		alias = conf.gvm->vhs.find_alias(internal, path, len);
		if (!alias) {
			return false;
		}
	}
	char* doc_root = nullptr;
	if (!kgl_is_absolute_path(alias->to)) {
		doc_root = KFileName::concatDir(this->doc_root.c_str(), alias->to);
		exsit = file->setName(doc_root, path + alias->path_len, flag);
		free(doc_root);
	} else {
		exsit = file->setName(alias->to, path + alias->path_len, flag);
	}
	return true;
}
kgl_auto_cstr KVirtualHost::alias(bool internal, const char* path) {
	auto len = strlen(path);
	auto alias = find_alias(internal, path, len);
	if (!alias) {
		if (!inherit) {
			return nullptr;
		}
		alias = conf.gvm->vhs.find_alias(internal, path, len);
		if (!alias) {
			return nullptr;
		}
	}
	char* result = alias->matched(path, len);
	if (kgl_is_absolute_path(result)) {
		return kgl_auto_cstr(result);
	}
	auto result2 = KFileName::concatDir(this->doc_root.c_str(), result);
	free(result);
	return kgl_auto_cstr(result2);
}
KBaseRedirect* KVirtualHost::refsPathRedirect(const char* path, int path_len) {
	auto rd = refs_path_redirect(path, path_len);
	if (rd != nullptr || !inherit) {
		return rd;
	}
	return conf.gvm->vhs.refs_path_redirect(path, path_len);
}
KFetchObject* KVirtualHost::findPathRedirect(KHttpRequest* rq, KFileName* file, const char* path, bool fileExsit, bool& result) {
	auto path_len = strlen(path);
	auto fo = find_path_redirect(rq, file, path, path_len, fileExsit, result);
	if (result || !inherit) {
		return fo;
	}
	return conf.gvm->vhs.find_path_redirect(rq, file, path, path_len, fileExsit, result);
}
KFetchObject* KVirtualHost::findFileExtRedirect(KHttpRequest* rq, KFileName* file, bool fileExsit, bool& result) {
	char* file_ext = (char*)file->getExt();
	auto fo = find_file_redirect(rq, file, file_ext, fileExsit, result);
	if (result || !inherit) {
		return fo;
	}
	return conf.gvm->vhs.find_file_redirect(rq, file, file_ext, fileExsit, result);
}
#ifdef ENABLE_BASED_PORT_VH
void KVirtualHost::get_bind_html(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>" << klang["bind"] << "</td></tr>";
	int j = 0;
	for (auto it = binds.begin(); it != binds.end(); ++it, ++j) {
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=bind_del&index=" << j;
		s << "&" << url << "';}\">" << LANG_DELETE << "</a>]</td><td>";
		s << (*it) << "</td></tr>";
	}
	s << "</table>";
	s << "<form action='/vhbase?action=bind_add&" << url << "' method='post'>";
	s << klang["bind"] << ":<input name='bind' value='' placeHolder= '*:443s'>";
	s << "<input type='submit' value='" << LANG_ADD << "'>";
	s << "</form>";
}
#endif
void KVirtualHost::get_host_html(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>host</td><td>dir</td><td>ssl</td></tr>";
	int j = 0;
	for (auto it = hosts.begin(); it != hosts.end(); ++it, ++j) {
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=host_del&index=" << j;
		s << "&" << url << "';}\">" << LANG_DELETE << "</a>]</td><td>";
		if ((*it)->wide) {
			s << "*";
			if (*(*it)->host) {
				s << ".";
			}
		}
		s << (*it)->host << "</td><td>" << ((*it)->dir ? (*it)->dir : "") << "</td><td>";
#ifdef ENABLE_SVH_SSL
		if ((*it)->ssl_ctx) {
			s << "ssl ok";
		}
#endif
		s << "</td></tr>";
	}
	s << "</table>";
	s << "<form action='/vhbase?action=host_add&" << url << "' method='post'>";
	s << "<div>host:<input name='host' value=''></div>";
	s << "<div>dir:<input name='dir'></div>";
#ifdef ENABLE_SVH_SSL
	s << "<div>certificate:<input name='certificate'></div>";
	s << "<div>certificate_key:<input name='certificate_key'></div>";
#endif
	s << "<input type='submit' value='" << LANG_ADD << "'></form>";
}
void KVirtualHost::closeToken(Token_t token) {
	if (token == NULL) {
		return;
	}
#ifdef _WIN32
	CloseHandle(token);
#endif
}
#ifdef ENABLE_VH_RUN_AS
void KVirtualHost::createToken(Token_t token) {
#ifdef _WIN32
	HANDLE curThread = GetCurrentProcess();
	OpenProcessToken(curThread, TOKEN_ALL_ACCESS, &token);
	CloseHandle(curThread);
#else
	token[0] = getuid();
	token[1] = getgid();
#endif
}
Token_t KVirtualHost::getProcessToken(bool& result) {
	return createToken(result);
}
Token_t KVirtualHost::createToken(bool& result) {
#ifdef _WIN32
	if (user.empty()) {
		result = true;
		return NULL;
	}
	HANDLE token = NULL;
	result = (LogonUser(user.c_str(),
		".",
		group.c_str(),
		LOGON32_LOGON_INTERACTIVE,
		LOGON32_PROVIDER_DEFAULT,
		&token) == TRUE);
	return token;
#else
	result = true;
	return (Token_t)&id;
#endif
}
#ifdef _WIN32
HANDLE KVirtualHost::logon(bool& result) {
	lock.Lock();
	if (!logoned) {
		logoned = true;
		assert(token == NULL);
		bool create_token_result = false;
		token = createToken(create_token_result);
		logonresult = create_token_result;
	}
	lock.Unlock();
	result = logonresult;
	if (result && token) {
		result = ImpersonateLoggedOnUser(token) == TRUE;
	}
	return token;
}
#endif
bool KVirtualHost::setRunAs(const KString& user, const KString& group) {
	//if (user == NULL || strlen(group) == 0) {
	this->user = user;
	this->group = group;
#ifdef _WIN32
	if (user.empty()) {
		this->group = "";
	}
	return true;
#else
	struct stat buf;
	memset(&buf, 0, sizeof(buf));
	if (user == "-" || group == "-") {
		if (stat(doc_root.c_str(), &buf) != 0) {
			klog(KLOG_ERR, "cann't stat doc_root [%s]\n", doc_root.c_str());
		}
	}
	if (user == "-") {
		id[0] = buf.st_uid;
	} else {
		if (!name2uid(user.c_str(), id[0], id[1])) {
			return false;
		}
	}
	if (group == "-") {
		id[1] = buf.st_gid;
	} else {
		return name2gid(group.c_str(), id[1]);
	}
	return true;
#endif
}
#endif
bool KVirtualHost::setDocRoot(const KString& docRoot) {
	if (docRoot.empty()) {
		return false;
	}
	doc_root = get_vh_full_doc_root(docRoot);
	return true;
}
#ifdef ENABLE_VH_LOG_FILE
void KVirtualHost::parse_log_config(const KXmlAttribute& attr) {
	auto path = attr["log_file"];
	if (path.empty()) {
		return;
	}
	logFile = path;
	assert(logger == NULL);
	if (path[0] != '|' && !isAbsolutePath(path.c_str())) {
		path = doc_root + path;
	} else {
#ifdef _WIN32
		if (path[0] == '/' && path != "/nolog") {
			path = conf.diskName + path;
		}
#endif
	}
	auto locker = logManage.get_locker();
	auto it = logManage.logs.find(path);
	if (it == logManage.logs.end()) {
		logger = new KLogElement;
		logger->setPath(path);
		logger->place = LOG_FILE;
		logManage.logs.insert(std::pair<KString, KLogElement*>(path, logger));
	} else {
		logger = (*it).second;
	}
	logger->setRotateTime(attr("log_rotate_time"));
	logger->rotate_size = get_size(attr("log_rotate_size"));
	logger->logs_day = attr.get_int("logs_day");
	logger->logs_size = get_size(attr("logs_size"));
	logger->log_handle = (attr["log_handle"] == "on" || attr["log_handle"] == "1");
	logger->mkdir_flag = (attr["log_mkdir"] == "on" || attr["log_handle"] == "1");
#ifdef ENABLE_VH_RUN_AS
#ifndef _WIN32
	logger->uid = id[0];
	logger->gid = id[1];
#endif
#endif
	logger->addRef();
}
#endif
#ifdef ENABLE_USER_ACCESS
void KVirtualHost::setAccess(const KString& access_file) {
	this->user_access = access_file;
}
void KVirtualHost::access_config_listen(kconfig::KConfigTree* tree, KVirtualHost* ov) {
	KStringBuf s;
	get_access_file(s);
	if (ov) {
		if (ov->user_access == user_access) {
			return;
		}
		if (!ov->user_access.empty() && ov->user_access != "-") {
			kconfig::remove_config_file(s.str().data());
		}
	}
	if (!user_access.empty() && user_access != "-") {
		KStringBuf accessFile;
		if (isAbsolutePath(user_access.c_str())) {
			accessFile << user_access;
		} else {
			accessFile << doc_root;
			accessFile << user_access;
		}

		kconfig::add_config_file(s.str().data(), accessFile.str().data(), tree, kconfig::KConfigFileSource::Vh);
	}
	return;
}
#endif
void KVirtualHost::copy_to(KVirtualHost* vh) {
	for (auto&& file_rd : redirects) {
		if (file_rd.second->rd) {
			file_rd.second->rd->add_ref();
		}
		KBaseRedirect* br = new KBaseRedirect(file_rd.second->rd, file_rd.second->confirm_file);
#ifdef ENABLE_UPSTREAM_PARAM
		br->params = file_rd.second->params;
#endif
		br->allowMethod = file_rd.second->allowMethod;
		vh->redirects.insert(std::pair<KString, KBaseRedirect*>(file_rd.first, br));
	}
	for (auto&& path : pathRedirects) {
		if (path->rd) {
			path->rd->add_ref();
		}
		KPathRedirect* prd = new KPathRedirect(path->path, path->rd);
		prd->confirm_file = path->confirm_file;
#ifdef ENABLE_UPSTREAM_PARAM
		prd->params = path->params;
#endif
		prd->allowMethod = path->allowMethod;
		vh->pathRedirects.push_back(prd);
	}
	vh->indexFiles = indexFiles;
	vh->errorPages = errorPages;
	vh->aliass = aliass;
	vh->access[0] = access[0];
	vh->access[1] = access[1];
#if 0
	vh->binds = binds;
	for (auto&& item : hosts) {
		KSubVirtualHost* svh = new KSubVirtualHost(vh);
		svh->setHost(item->host);
		svh->setDocRoot(vh->doc_root.c_str(), item->dir);
		vh->hosts.push_back(svh);
	}
#endif
}
bool KVirtualHost::loadApiRedirect(KApiPipeStream* st, int workType) {
	lock.Lock();
	for (auto it = pathRedirects.begin(); it != pathRedirects.end(); it++) {
		if (!loadApiRedirect((*it)->rd, st, workType)) {
			lock.Unlock();
			return false;
		}
	}
	for (auto it2 = redirects.begin(); it2 != redirects.end(); it2++) {
		if (!loadApiRedirect((*it2).second->rd, st, workType)) {
			lock.Unlock();
			return false;
		}
	}
	lock.Unlock();
	return true;
}
bool KVirtualHost::loadApiRedirect(KRedirect* rd, KApiPipeStream* st,
	int workType) {
	if (rd == NULL) {
		return true;
	}
	if (strcmp(rd->getType(), "api") == 0) {
		KApiRedirect* ard = static_cast<KApiRedirect*> (rd);
		KExtendProgramString ds(ard->name.c_str(), this);
		if (ard->type == workType && !st->isLoaded(ard)) {
			ard->preLoad(&ds);
			bool result = st->loadApi(ard);
			if (result) {
				ds.setPid(st->process.getProcessId());
				ard->postLoad(&ds);
			} else {
				klog(KLOG_ERR, "cann't load api [%s]\n", ard->name.c_str());
			}
			return result;
		}
	}
	return true;
}
#ifdef ENABLE_VH_RS_LIMIT
void KVirtualHost::setSpeedLimit(int speed_limit, KVirtualHost* ov) {
	lock.Lock();
	this->speed_limit = speed_limit;
	if (speed_limit == 0) {
		if (sl) {
			sl->release();
			sl = NULL;
		}
	} else {
		if (sl == NULL) {
			if (ov) {
				sl = ov->sl;
			}
			if (sl) {
				sl->addRef();
			} else {
				sl = new KSpeedLimit();
			}
		}
		sl->setSpeedLimit(speed_limit);
	}
	lock.Unlock();
}
void KVirtualHost::setSpeedLimit(const char* speed_limit_str, KVirtualHost* ov) {
	setSpeedLimit((int)get_size(speed_limit_str), ov);
}
int KVirtualHost::GetConnectionCount() {
	if (cur_connect) {
		return cur_connect->getConnectionCount();
	}
	return katom_get((void*)&ref) - VH_REFS_CONNECT_DELTA;
}
#endif
#ifdef ENABLE_VH_QUEUE
unsigned KVirtualHost::getWorkerCount() {
	if (queue) {
		return queue->getWorkerCount();
	}
	return 0;
}
unsigned KVirtualHost::getQueueSize() {
	if (queue) {
		return queue->getQueueSize();
	}
	return 0;
}
#endif
#ifdef ENABLE_VH_RUN_AS
void KVirtualHost::KillAllProcess() {
	for (size_t i = 0; i < apps.size(); i++) {
		conf.gam->killCmdProcess(apps[i]);
	}
}
bool KVirtualHost::caculateNeedKillProcess(KVirtualHost* ov) {

	if (doc_root != ov->doc_root) {
		return true;
	}
	if (envs.size() > ov->envs.size()) {
		return true;
	}
	//check env change
	for (auto it3 = envs.begin(); it3 != envs.end(); it3++) {
		auto it4 = ov->envs.find((*it3).first);
		if (it4 == ov->envs.end()) {
			return true;
		}
		if (strcmp((*it3).second, (*it4).second) != 0) {
			return true;
		}
	}
	return false;
}
#endif
KString KVirtualHost::getApp(KHttpRequest* rq) {
	if (app <= 0) {
		return getUser();
	}
	kassert((int)apps.size() == (int)app);
	//todo:以后根据ip做hash
	int index = (ip_hash ? ksocket_addr_hash(rq->sink->get_peer_addr()) : rand()) % app;
	//printf("get vh=[%p] app=[%s]\n",this,apps[index].c_str());
	return apps[index];
}
void KVirtualHost::setApp(int app) {
	if (app <= 0 || app > 512) {
		app = 1;
	}
	apps.clear();
	this->app = (uint8_t)app;
	KStringBuf s;
	for (int i = 0; i < app; i++) {
		if (app_share) {
			s << getUser();
		} else {
			s << name;
		}
		s << ":" << (i + 1);
		apps.push_back(s.reset());
	}
	apps.shrink_to_fit();
}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
KString KVirtualHost::get_cert_file() const {
	return KSslCertificate::get_cert_file(doc_root);
}
KString KVirtualHost::get_key_file() const {
	return KSslCertificate::get_key_file(doc_root);
}
bool KVirtualHost::setSSLInfo(const KString& certfile, const KString& keyfile, const KString& cipher, const KString& protocols) {
	this->cert_file = certfile;
	this->key_file = keyfile;
	this->cipher = cipher;
	this->protocols = protocols;
	if (ssl_ctx) {
		//if an old exsit, release it;
		kgl_release_ssl_ctx(ssl_ctx);
		ssl_ctx = nullptr;
	}
	if (cert_file.empty()) {
		return false;
	}
	ssl_ctx = refs_ssl_ctx();
	if (ssl_ctx) {
		kgl_release_ssl_ctx(ssl_ctx);
		return true;
	}
	return false;
}
#endif
bool KVirtualHost::parse_xml(const khttpd::KXmlNodeBody* body, KVirtualHost* ov) {

	auto attr = body->attr();
	envs.clear();
	parseEnv(attr("envs"));
	setDocRoot(attr["doc_root"]);
	browse = (attr["browse"] == "on" || attr["browse"] == "1");

	inherit = (attr["inherit"] == "on" || attr["inherit"] == "1");
	setAccess(attr["access"]);
	htaccess = attr["htaccess"];
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	KString certfile;
	KString keyfile;
	KString cipher;
	KString protocols;
#ifdef ENABLE_HTTP2
	alpn = 0;
	if (!attr["alpn"].empty()) {
		alpn = attr.get_int("alpn");
	} else {
		if (attr.get_int("http2") == 1) {
			KBIT_SET(alpn, KGL_ALPN_HTTP2);
		}
#ifdef ENABLE_HTTP3
		if (attr.get_int("http3") == 1) {
			KBIT_SET(alpn, KGL_ALPN_HTTP3);
		}
#endif
	}
#endif
	early_data = attr.get_int("early_data") == 1;
	setSSLInfo(attr["certificate"], attr["certificate_key"], attr["cipher"], attr["protocols"]);
#endif
#ifdef ENABLE_VH_RUN_AS
	setRunAs(attr["user"], attr["group"]);
#endif
#ifndef _WIN32
	chroot = (attr["chroot"] == "on" || attr["chroot"] == "1");
#endif
	app_share = (attr["app_share"] == "on" || attr["app_share"] == "1");
	setApp(attr.get_int("app"));
	ip_hash = (attr["ip_hash"] == "on" || attr["ip_hash"] == "1");
#ifdef ENABLE_VH_LOG_FILE
	parse_log_config(attr);
#endif
#ifdef ENABLE_VH_RS_LIMIT
	setSpeedLimit(attr("speed_limit"), ov);
	max_connect = attr.get_int("max_connect");
	initConnect(ov);
#endif
#ifdef ENABLE_BLACK_LIST
	if (ov) {
		assert(ov->blackList);
		blackList = ov->blackList;
		ov->blackList->addRef();
	} else {
		blackList = new KIpList();
	}
#endif
#ifdef ENABLE_VH_FLOW
	setFlow(attr.get_int("fflow") == 1, ov);
#endif
#ifdef ENABLE_VH_QUEUE
	initQueue((unsigned)attr.get_int("max_worker"), (unsigned)attr.get_int("max_queue"), ov);
#endif
	SetStatus(attr.get_int("status"));
	if (ov != nullptr) {
		auto locker = ov->get_locker();
		ov->copy_to(this);
	}
	//parse host
	auto host_xml = kconfig::find_child(body, _KS("host"));
	if (host_xml) {
		for (uint32_t index = 0;; ++index) {
			auto body = host_xml->get_body(index);
			if (!body) {
				break;
			}
			auto svh = parse_host(body);
			if (!svh) {
				continue;
			}
			hosts.push_back(svh);
		}
	}
	//parse bind
	auto bind_xml = kconfig::find_child(body, _KS("bind"));
	if (bind_xml) {
		for (uint32_t index = 0;; ++index) {
			auto body = bind_xml->get_body(index);
			if (!body) {
				break;
			}
			auto bind = body->get_text();
			if (!bind) {
				continue;
			}
			if (bind[0] == '!') {
				binds.push_back(bind.substr(1));
			} else if (bind.find(':') != KString::npos) {
				binds.push_back(bind);
			} else {
				KStringBuf s;
				s << "*:" << bind;
				binds.push_back(s.c_str());
			}
		}
	}
	return true;
}
bool KVirtualHost::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	if (KBaseVirtualHost::on_config_event(tree, ev)) {
		return true;
	}
	auto xml = ev->get_xml();
	//printf("***********vh xml->tag=[%s]\n", xml->get_tag().c_str());
	auto attr = xml->attributes();
	switch (ev->type) {
	case kconfig::EvSubDir | kconfig::EvNew:
		if (xml->is_tag(_KS("request"))) {
			auto locker = get_locker();
			if (ev->type == (kconfig::EvSubDir | kconfig::EvNew)) {
				if (!access[REQUEST]) {
					access[REQUEST].reset(new KAccess(false, REQUEST));
				}
				bind_access_config(tree, access[REQUEST].get());
			}
			access[REQUEST]->parse_config(attr);
			return true;
		}
		if (xml->is_tag(_KS("response"))) {
			auto locker = get_locker();
			if (ev->type == (kconfig::EvSubDir | kconfig::EvNew)) {
				if (!access[RESPONSE]) {
					access[RESPONSE].reset(new KAccess(false, RESPONSE));
				}
				bind_access_config(tree, access[RESPONSE].get());
			}
			access[RESPONSE]->parse_config(attr);
			return true;
		}
		//[[fallthrough]]
	case kconfig::EvSubDir | kconfig::EvUpdate:
	{
#if 0
		if (xml->is_tag(_KS("host"))) {
			/* notice parent xml tag event. */
			return false;
		}
		if (xml->is_tag(_KS("bind"))) {
			std::list<KString> binds;
			for (uint32_t index = 0;; ++index) {
				auto body = xml->get_body(index);
				if (!body) {
					break;
				}
				auto bind = body->get_text();
				if (!bind) {
					continue;
				}
				if (bind[0] == '!') {
					binds.push_back(bind.substr(1));
				} else if (bind.find(':') != KString::npos) {
					binds.push_back(bind);
				} else {
					KStringBuf s;
					s << "*:" << bind;
					binds.push_back(s.c_str());
				}
			}
			conf.gvm->updateVirtualHost(this, binds);
			return true;

			/* notice parent xml tag event. */
			//return false;
		}
#endif
	}
	break;
	case kconfig::EvSubDir | kconfig::EvRemove:
#if 0
		if (xml->is_tag(_KS("host"))) {
			/* notice parent xml tag event. */
			return false;
		}
		if (xml->is_tag(_KS("bind"))) {
			//*
			std::list<KString> binds;
			conf.gvm->updateVirtualHost(this, binds);
			return true;
			//*/
			/* notice parent xml tag event. */
			//return false;
		}
#endif
		break;
	}
	return false;
}