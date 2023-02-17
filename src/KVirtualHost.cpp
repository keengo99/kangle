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
#include "KTempleteVirtualHost.h"
#include "extern.h"
#include "KAcserverManager.h"

volatile bool cur_config_vh_db = false;
using namespace std;
const std::string slashString(const std::string &str)
{
	std::stringstream s;
	if(str.size()==0){
		return "";
	}
	int length = (int)str.size();
	if(str[length-1]=='\\'){
		s << str << "\\";
		return s.str();
	}
	return str;
}
KVirtualHost::KVirtualHost() {
	flags = 0;
	db = cur_config_vh_db;
#ifdef ENABLE_USER_ACCESS
	access[REQUEST].type = REQUEST;
	access[REQUEST].qName = "request";
	access[RESPONSE].type = RESPONSE;
	access[RESPONSE].qName = "response";
#endif
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
	max_queue = 0;
	max_worker = 0;
#endif
	inherit = true;
	if (!db) {
		ext = cur_config_ext;
	}
	tvh = NULL;
	app_share = 1;
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	ssl_ctx = NULL;
#endif
}
KVirtualHost::~KVirtualHost() {
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	if(token) {
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
	list<KSubVirtualHost *>::iterator it2;
	for (it2 = hosts.begin(); it2 != hosts.end(); it2++) {
		delete (*it2);
	}
#ifdef ENABLE_VH_RS_LIMIT
	if (sl) {
		sl->release();
	}
	if(cur_connect){
		cur_connect->release();
	}
#endif
#ifdef ENABLE_VH_FLOW
	if (flow) {
		flow->release();
	}
#endif
#ifdef ENABLE_VH_QUEUE
	if(queue){
		queue->release();
	}
#endif
	if (tvh) {
		tvh->destroy();
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if(ssl_ctx){
		kgl_release_ssl_ctx(ssl_ctx);
	}
#endif
}
bool KVirtualHost::isPathRedirect(KHttpRequest *rq, KFileName *file,
		bool fileExsit, KRedirect *rd) {
	bool result = false;
	int path_len = (int)strlen(rq->sink->data.url->path);
	lock.Lock();
	list<KPathRedirect *>::iterator it2;
	for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		if ((*it2)->match(rq->sink->data.url->path,path_len) 
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
KFetchObject *KVirtualHost::findDefaultRedirect(KHttpRequest *rq,
		KFileName *file, bool fileExsit) {
	KRedirectSource*fo = NULL;
	lock.Lock();
	if (defaultRedirect 
		&& defaultRedirect->rd
		&& defaultRedirect->allowMethod.matchMethod(rq->sink->data.meth)) {
		switch (defaultRedirect->confirmFile) {
		case 0:
			//不确认文件是否存在
			fo = defaultRedirect->rd->makeFetchObject(rq, file);
			break;
		case 1:
			//确认文件存在
			if (fileExsit) {
				fo = defaultRedirect->rd->makeFetchObject(rq, file);
			}
			break;
		case 2:
			//确认文件不存在
			if (!fileExsit) {
				fo = defaultRedirect->rd->makeFetchObject(rq, file);
			}
			break;
		}
	}
	if (fo) {
		fo->bindBaseRedirect(defaultRedirect);
	}
	lock.Unlock();
	return fo;
}
KBaseRedirect* KVirtualHost::refsPathRedirect(const char* path, int path_len)
{
	KLocker locker(&lock);
	for (auto it = pathRedirects.begin(); it != pathRedirects.end(); it++) {
		if ((*it)->match(path, path_len)) {
			(*it)->addRef();
			return (*it);
		}
	}
	return NULL;
}
KFetchObject *KVirtualHost::findPathRedirect(KHttpRequest *rq, KFileName *file,const char *path,
		bool fileExsit, bool &result) {
	KRedirectSource*fo = NULL;
	int path_len = (int)strlen(path);
	KLocker locker(&lock);
	list<KPathRedirect *>::iterator it2;
	for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		if ((*it2)->match(path,path_len) && (*it2)->allowMethod.matchMethod(rq->sink->data.meth)) {
			if ((*it2)->MatchConfirmFile(fileExsit)) {
				result = true;
				if ((*it2)->rd) {
					fo = (*it2)->rd->makeFetchObject(rq, file);
					fo->bindBaseRedirect((*it2));
				}
				break;
			}
		}
	}
	return fo;
}
KFetchObject *KVirtualHost::findFileExtRedirect(KHttpRequest *rq,
		KFileName *file, bool fileExsit, bool &result) {
	KRedirectSource*fo = NULL;
	char *file_ext = (char *) file->getExt();
	lock.Lock();
	if (file_ext) {
		std::map<char *, KBaseRedirect *, lessf>::iterator it = redirects.find((char *) file->getExt());
		if (it != redirects.end() && (*it).second->allowMethod.matchMethod(rq->sink->data.meth)) {
			if ((*it).second->MatchConfirmFile(fileExsit)) {
				result = true;
				if ((*it).second->rd) {
					fo = (*it).second->rd->makeFetchObject(rq, file);
					fo->bindBaseRedirect((*it).second);
				}
			}
		}
	}
	lock.Unlock();
	return fo;
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
	OpenProcessToken(curThread,TOKEN_ALL_ACCESS,&token);
	CloseHandle(curThread);
#else
	token[0] = getuid();
	token[1] = getgid();
#endif
}
Token_t KVirtualHost::getProcessToken(bool &result) {
	return createToken(result);
}
Token_t KVirtualHost::createToken(bool &result) {
#ifdef _WIN32
	if(user.empty()) {
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
	return (Token_t) &id;
#endif
}
#ifdef _WIN32
HANDLE KVirtualHost::logon(bool &result)
{
	lock.Lock();
	if(!logoned) {
		logoned = true;
		assert(token==NULL);
		bool create_token_result = false;
		token = createToken(create_token_result);
		logonresult = create_token_result;
	}
	lock.Unlock();
	result = logonresult;
	if(result && token) {
		result = ImpersonateLoggedOnUser(token) == TRUE;
	}
	return token;
}
#endif
bool KVirtualHost::setRunAs(std::string user, std::string group) {
	//if (user == NULL || strlen(group) == 0) {
	this->user = user;
	this->group = group;
#ifdef _WIN32
	if(user.size()==0) {
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
bool KVirtualHost::setDocRoot(std::string &docRoot) {
	if (docRoot.size() == 0) {
		return false;
	}
	doc_root = docRoot;
	if (!isAbsolutePath(doc_root.c_str())) {
		doc_root = conf.path + doc_root;
	} else {
#ifdef _WIN32
		if(doc_root[0]=='/'){
			doc_root = conf.diskName + doc_root;
		}
#endif
	}
	pathEnd(doc_root);
	return true;
}
#ifdef ENABLE_VH_LOG_FILE
void KVirtualHost::setLogFile(std::string &path, std::map<std::string,
		std::string>&attribute) {
	if (path.empty()) {
		return;
	}
	logFile = path;
	assert(logger==NULL);
	if (!isAbsolutePath(path.c_str())) {
		path = doc_root + path;
	}else{
#ifdef _WIN32
		if(path[0]=='/' && path!="/nolog"){
			path = conf.diskName + path;
		}
#endif
	}
	std::map<string, KLogElement *>::iterator it;
	logManage.lock.Lock();
	it = logManage.logs.find(path);
	if (it == logManage.logs.end()) {
		logger = new KLogElement;
		logger->setPath(path);
		logger->place = LOG_FILE;
		logManage.logs.insert(pair<string, KLogElement *> (path, logger));
	} else {
		logger = (*it).second;

	}
	//	if (attribute["log_rotate_time"].size() > 0) {
	logger->setRotateTime(attribute["log_rotate_time"].c_str());
	//}
	//	if (attribute["log_rotate_size"].size() > 0) {
	logger->rotate_size = get_size(attribute["log_rotate_size"].c_str());
	logger->logs_day = atoi(attribute["logs_day"].c_str());
	logger->logs_size = get_size(attribute["logs_size"].c_str());
	//}
#ifdef ENABLE_VH_RUN_AS
	logger->uid = id[0];
	logger->gid = id[1];
#endif
	if (strcasecmp(attribute["log_mkdir"].c_str(), "on") == 0) {
		logger->mkdir_flag = true;
	} else {
		logger->mkdir_flag = false;
	}
	if (strcasecmp(attribute["log_handle"].c_str(), "off") == 0) {
		logger->log_handle = false;
	} else {
		logger->log_handle = true;
	}
	logger->addRef();
	logManage.lock.Unlock();
}
void KVirtualHost::setLogFile(KAttributeHelper *ah, KVirtualHost *tm) {
	std::string path;
	if (!ah->getValue("log_file", path) && tm) {
		path = tm->logFile;
	}
	if (path.empty()) {
		return;
	}
	logFile = path;
	assert(logger==NULL);
	if (path[0]!='|' && !isAbsolutePath(path.c_str())) {
		path = doc_root + path;
	}else{
#ifdef _WIN32
		if (path[0]=='/' && path!="/nolog") {
			path = conf.diskName + path;
		}
#endif
	}
	std::map<string, KLogElement *>::iterator it;
	logManage.lock.Lock();
	it = logManage.logs.find(path);
	if (it == logManage.logs.end()) {
		logger = new KLogElement;
		logger->setPath(path);
		logger->place = LOG_FILE;
		logManage.logs.insert(pair<string, KLogElement *> (path, logger));
	} else {
		logger = (*it).second;
	}
	string value;
	if (!ah->getValue("log_rotate_time", value) && tm && tm->logger) {
		tm->logger->getRotateTime(value);
	}
	logger->setRotateTime(value.c_str());
	if (ah->getValue("log_rotate_size", value)) {
		logger->rotate_size = get_size(value.c_str());
	} else if (tm && tm->logger) {
		logger->rotate_size = tm->logger->rotate_size;
	}
	if(ah->getValue("logs_day",value)){
		logger->logs_day = atoi(value.c_str());
	}else if(tm && tm->logger){
		logger->logs_day = tm->logger->logs_day;
	}
	if(ah->getValue("logs_size",value)){
		logger->logs_size = get_size(value.c_str());
	}else if(tm && tm->logger){
		logger->logs_size = tm->logger->logs_size;
	}
	if (ah->getValue("log_handle", value)) {
		if (strcasecmp(value.c_str(), "off") == 0 || value == "0") {
			logger->log_handle = false;
		} else {
			logger->log_handle = true;
		}
	} else if (tm && tm->logger) {
		logger->log_handle = tm->logger->log_handle;
	}
	if (ah->getValue("log_mkdir", value)) {
		if (strcasecmp(value.c_str(), "on") == 0 || value == "1") {
			logger->mkdir_flag = true;
		} else {
			logger->mkdir_flag = false;
		}
	} else if (tm && tm->logger) {
		logger->mkdir_flag = tm->logger->mkdir_flag;
	}
#ifdef ENABLE_VH_RUN_AS
	logger->uid = id[0];
	logger->gid = id[1];
#endif
	logger->addRef();
	logManage.lock.Unlock();
}
#endif
#ifdef ENABLE_USER_ACCESS
bool KVirtualHost::saveAccess()
{
	if (user_access.size()==0) {
		return false;
	}
	if (user_access=="-") {
		std::string errMsg;
		conf.gvm->saveConfig(errMsg);
		return true;
	}
	std::string accessFile = doc_root;
	accessFile += user_access;
	stringstream s;
	s << "<config>\n";
	for(int i=0;i<2;i++){
		access[i].buildXML(s,CHAIN_XML_DETAIL);
	}
	s << "</config>\n";
	KFile fp;
	if (!fp.open(accessFile.c_str(),fileWrite,KFILE_NOFOLLOW)) {
		klog(KLOG_ERR,"Cann't save to access file [%s]\n",accessFile.c_str());
		return false;
	}
	fp.write(s.str().c_str(),(int)s.str().size());
	fp.close();
	return true;
}
int KVirtualHost::checkRequest(KHttpRequest *rq, KFetchObject **fo) {
	if (!loadAccess()) {
		return JUMP_ALLOW;
	}
	return access[REQUEST].check(rq, NULL, fo);
}
int KVirtualHost::checkResponse(KHttpRequest *rq, KFetchObject** fo)
{
	if (user_access.empty()) {
		return JUMP_ALLOW;
	}
	return access[RESPONSE].check(rq,rq->ctx.obj, fo);
}
int KVirtualHost::checkPostMap(KHttpRequest *rq, KFetchObject** fo)
{
	if(user_access.size()==0){
		return JUMP_ALLOW;
	}
	return access[RESPONSE].checkPostMap(rq,rq->ctx.obj, fo);
}
void KVirtualHost::setAccess(std::string access_file)
{
	this->user_access = access_file;
}
bool KVirtualHost::loadAccess(KVirtualHost *vh) {
	if (user_access.empty()) {
		return false;
	}
	if (access_file_loaded) {
		return true;
	}
	access_file_loaded = true;
	std::string err_msg;
	if (user_access=="-") {
		std::stringstream s;
		access[0].newTable(BEGIN_TABLE, err_msg);
		access[1].newTable(BEGIN_TABLE, err_msg);
		if (vh) {
			s << "<config>\n";
			for (int i=0;i<2;i++) {
				vh->access[i].buildXML(s,(CHAIN_XML_DETAIL|CHAIN_SKIP_EXT));
			}
			s << "</config>\n";			
			KAccessParser parser;
			parser.parseString(s.str().c_str(), &this->access[0]);
		}
		return true;
	}
	std::string accessFile;
	if (isAbsolutePath(user_access.c_str())) {
		accessFile = user_access;
	} else {
		accessFile = doc_root;
		accessFile += user_access;
	}
	struct _stati64 buf;	
	if (lstat(accessFile.c_str(), &buf) != 0 || !S_ISREG(buf.st_mode)) {
		for (int i=0;i<2;i++) {
				this->access[i].destroy();
				this->access[i].newTable(BEGIN_TABLE, err_msg);
		}		
		return true;
	}
	KAccess access[2];
	KAccessParser parser;
	access[REQUEST].type = REQUEST;
	access[REQUEST].qName = "request";
	access[RESPONSE].type = RESPONSE;
	access[RESPONSE].qName = "response";
	access[0].newTable(BEGIN_TABLE, err_msg);
	access[1].newTable(BEGIN_TABLE, err_msg);
	parser.parseFile(accessFile, &access[0]);
	for (int i = 0; i < 2; i++) {
		this->access[i].copy(access[i]);
	}
	return true;
}
#endif
void KVirtualHost::buildXML(std::stringstream &s) {
	lock.Lock();
	//	s << "<vh ";
	if (name.size() > 0) {
		s << "name='" << name << "' ";
	}
	s << "doc_root='" << GetDocumentRoot() << "' ";
#ifdef ENABLE_VH_LOG_FILE
	if (logger) {
		s << "log_file='" << logFile << "'";
		logger->buildXML(s);
	}
#endif
	s << " inherit='" << (inherit ? "on" : "off") << "'";
#ifdef ENABLE_VH_RUN_AS
	if (user.size() > 0) {
		s << " user='" << user << "'";
	}
	if (group.size() > 0) {
#ifndef _WIN32
		s << " group='";
#else
		s << " password='";
#endif
		s << group << "'";
	}
	if (app>0) {
		s << " app='" << (int)app << "'";
	}
	if (ip_hash) {
		s << " ip_hash='1'";
	}
	if (app_share!=1) {
		s << " app_share='" << (int)app_share << "'";
	}
#ifndef _WIN32
	if (chroot) {
		s << " chroot='1'";
	}
#endif
#endif
	if (browse) {
		s << " browse='on'";
	}
#ifdef ENABLE_USER_ACCESS
	if(user_access.size()>0){
		s << " access='" << user_access << "'";
	}
#endif
	if(htaccess.size()>0){
		s << " htaccess='" << htaccess << "'";
	}
#ifdef ENABLE_VH_RS_LIMIT
	if (max_connect > 0) {
		s << " max_connect='" << max_connect << "'";
	}
	if (speed_limit > 0) {
		s << " speed_limit='" << speed_limit << "'";
	}
#endif
#ifdef ENABLE_VH_FLOW
	if (fflow) {
		s << " fflow='" << (int)fflow << "'";
	}
#endif
#ifdef ENABLE_VH_QUEUE
	if(max_worker>0){
		s << " max_worker='" << max_worker << "'";
	}
	if(max_queue>0){
		s << " max_queue='" << max_queue << "'";
	}
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if (!cert_file.empty()) {
		s << " certificate='" << cert_file << "'";
	}
	if (!key_file.empty()) {
		s << " certificate_key='" << key_file << "'";
	}
	if (!cipher.empty()) {
		s << " cipher='" << cipher << "'";
	}
	if (!protocols.empty()) {
		s << " protocols='" << protocols << "'";
	}
#ifdef ENABLE_HTTP2
	if (alpn>0) {
		s << " alpn='" << (int)alpn << "'";
	}
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
	if (early_data) {
		s << " early_data='1'";
	}
#endif
#endif
	if (closed || tvh) {
		s << " status='" << (int)closed << "'";
	}
	if (tvh) {
		s << " templete='" << tvh->name << "'";
	}
	buildBaseXML(s);
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it5;
	for (it5=binds.begin();it5!=binds.end();it5++) {
		s << "<bind>" << (*it5) << "</bind>\n";
	}
#endif
	std::list<KSubVirtualHost *>::iterator it;
	for (it = hosts.begin(); it != hosts.end(); it++) {
		if ((*it)->fromTemplete) {
			continue;
		}
		s << "<host";
		if (strcmp((*it)->dir, "/") != 0
			//{{ent
#ifdef ENABLE_SVH_SSL
			|| (*it)->ssl_param!=NULL
#endif//}}
		){
			s << " dir='" << (*it)->dir;
			//{{ent
#ifdef ENABLE_SVH_SSL
			if ((*it)->ssl_param != NULL) {
				s << KGL_SSL_PARAM_SPLIT_CHAR << (*it)->ssl_param;
			}
#endif//}}
			s << "'";
		}
		s << ">" ;
		if ((*it)->wide) {
			s << "*";
		}
		s << (*it)->host ;
		s << "</host>\n";
	}
#ifdef ENABLE_USER_ACCESS
	if (user_access=="-") {
		for (int i=0;i<2;i++) {
			access[i].buildXML(s,0);
		}
	}
#endif
	lock.Unlock();
}
bool KVirtualHost::loadApiRedirect(KApiPipeStream *st, int workType) {
	lock.Lock();
	std::list<KPathRedirect *>::iterator it;
	for (it = pathRedirects.begin(); it != pathRedirects.end(); it++) {
		if (!loadApiRedirect((*it)->rd, st, workType)) {
			lock.Unlock();
			return false;
		}
	}
	std::map<char *, KBaseRedirect *, lessf>::iterator it2;
	for (it2 = redirects.begin(); it2 != redirects.end(); it2++) {
		if (!loadApiRedirect((*it2).second->rd, st, workType)) {
			lock.Unlock();
			return false;
		}
	}
	lock.Unlock();
	return true;
}
bool KVirtualHost::loadApiRedirect(KRedirect *rd, KApiPipeStream *st,
		int workType) {
	if(rd==NULL){
		return true;
	}
	if (strcmp(rd->getType(), "api") == 0) {
		KApiRedirect *ard = static_cast<KApiRedirect *> (rd);
		KExtendProgramString ds(ard->name.c_str(), this);
		if (ard->type == workType && !st->isLoaded(ard)) {
			ard->preLoad(&ds);
			bool result = st->loadApi(ard);
			if (result) {
				ds.setPid(st->process.getProcessId());
				ard->postLoad(&ds);
			} else {
				klog(KLOG_ERR,"cann't load api [%s]\n",ard->name.c_str());
			}
			return result;
		}
	}
	return true;
}
#ifdef ENABLE_VH_RS_LIMIT
void KVirtualHost::setSpeedLimit(int speed_limit,KVirtualHost *ov) {
	lock.Lock();
	this->speed_limit = speed_limit;
	if (speed_limit == 0) {
		if (sl) {
			sl->release();
			sl = NULL;
		}
	} else {
		if (sl == NULL) {
			if(ov){
				sl = ov->sl;
			}
			if(sl){
				sl->addRef();
			}else{
				sl = new KSpeedLimit();
			}
		}
		sl->setSpeedLimit(speed_limit);
	}
	lock.Unlock();
}
void KVirtualHost::setSpeedLimit(const char * speed_limit_str,KVirtualHost *ov) {
	setSpeedLimit((int) get_size(speed_limit_str),ov);
}
int KVirtualHost::GetConnectionCount() {
	if(cur_connect){
		return cur_connect->getConnectionCount();
	}
	return refs - VH_REFS_CONNECT_DELTA;
}
#endif
#ifdef ENABLE_VH_QUEUE
unsigned KVirtualHost::getWorkerCount()
{
	if(queue){
		return queue->getWorkerCount();
	}
	return 0;
}
unsigned KVirtualHost::getQueueSize()
{
	if(queue){
		return queue->getQueueSize();
	}
	return 0;
}
#endif
#ifdef ENABLE_VH_RUN_AS
void KVirtualHost::KillAllProcess()
{
	for (size_t i = 0; i < apps.size(); i++) {
		conf.gam->killCmdProcess(apps[i]);
	}
}
bool KVirtualHost::caculateNeedKillProcess(KVirtualHost *ov) {

	if (doc_root != ov->doc_root) {
		return true;
	}
	if (envs.size() > ov->envs.size()) {
		return true;
	}
	if(tvh != ov->tvh){
		return true;
	}
	//check env change
	map<char *, char *, lessp_icase>::iterator it3;
	for (it3 = envs.begin(); it3 != envs.end(); it3++) {
		map<char *, char *, lessp_icase>::iterator it4;
		it4 = ov->envs.find((*it3).first);
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
std::string KVirtualHost::getApp(KHttpRequest *rq)
{
	if (app<=0) {
		return getUser();
	}
	kassert((int)apps.size() == (int)app);
	//todo:以后根据ip做hash
	int index = (ip_hash? ksocket_addr_hash(rq->sink->get_peer_addr()):rand()) % app;
	//printf("get vh=[%p] app=[%s]\n",this,apps[index].c_str());
	return apps[index];
}
void KVirtualHost::setApp(int app)
{
	if (app<=0 || app>512) {
		app = 1;
	}
	apps.clear();
	this->app = (uint8_t)app;
	std::stringstream s;
	for (int i=0;i<app;i++) {
		s.str("");
		if (app_share) {
			s << getUser();
		} else {
			s << name;
		}
		s << ":" << (i+1);
		apps.push_back(s.str());
	}
}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
std::string KVirtualHost::get_cert_file()
{
	if (cert_file.empty()) {
		return cert_file;
	}
	if (cert_file[0] == '-') {
		return conf.path + (cert_file.c_str() + 1);
	}
	if (!isAbsolutePath(cert_file.c_str())) {
		return doc_root + cert_file;
	}
	return cert_file;
}
std::string KVirtualHost::get_key_file()
{
	if (key_file.empty()) {
		return key_file;
	}
	if (key_file[0] == '-') {
		return conf.path + (key_file.c_str() + 1);
	}
	if (!isAbsolutePath(key_file.c_str())) {
		return doc_root + key_file;
	}
	return key_file;
}
bool KVirtualHost::setSSLInfo(std::string certfile,std::string keyfile,std::string cipher,std::string protocols)
{
	this->cert_file = certfile;
	this->key_file = keyfile;
	this->cipher = cipher.c_str();
	this->protocols = protocols.c_str();
	if (ssl_ctx) {
		//if an old exsit, release it;
		kgl_release_ssl_ctx(ssl_ctx);
		ssl_ctx = nullptr;
	}
	return true;
}
#endif
