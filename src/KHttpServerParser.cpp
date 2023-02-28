/*
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
#include "katom.h"
#include <vector>
#include <string.h>
#include <stdlib.h>

#include <assert.h>
#include "KHttpServerParser.h"
#include "KHttpProxyFetchObject.h"
#include "KLogElement.h"
#include "KVirtualHost.h"
#include "KVirtualHostManage.h"
#include "KLogManage.h"
#include "KAcserverManager.h"
#include "KTempleteVirtualHost.h"
#include "KXml.h"
#include "utils.h"
#include "KVirtualHostDatabase.h"
#include "extern.h"
#include "kmalloc.h"
#ifndef HTTP_PROXY
using namespace std;
KHttpServerParser::KHttpServerParser() {
	virtualHost = NULL;
	cur_tvh = NULL;
	this->kaccess[0] = &::kaccess[0];
	this->kaccess[1] = &::kaccess[1];
}
KHttpServerParser::~KHttpServerParser() {
	if (virtualHost) {
		delete virtualHost;
	}
}
bool KHttpServerParser::parseString(const char *buf)
{
	KXml xml;
	bool result = false;
	xml.setEvent(this);
	try {		
		result = xml.parseString(buf);
	} catch (KXmlException &e) {
		klog(KLOG_ERR, "%s", e.what());
	}
	return result;
}
bool KHttpServerParser::parse(std::string file) {
	KXml xml;
	bool result = false;
	xml.setEvent(this);
	try {		
		result = xml.parseFile(file);
	} catch (KXmlException &e) {
		klog(KLOG_ERR, "%s", e.what());
	}
	return result;
}
bool KHttpServerParser::startElement(KXmlContext *context) {
	KConfig *c = (KConfig *)context->getData();
	KVirtualHostManage *vhm = (c?c->vm:conf.gvm);
	if (context->path == "config" && context->qName == "ssl") {
		ParseSsl(vhm, context->attribute);
		return true;
	}
	if (strcasecmp(context->qName.c_str(), "vh_database") == 0) {
		return vhd.parseAttribute(context->attribute);
	}
	if (strcasecmp(context->qName.c_str(), "vhs") == 0) {
		buildBaseVirtualHost(context->attribute, &vhm->globalVh);
		return true;
	} else if (strcasecmp(context->qName.c_str(), "vh") == 0) {
		assert(virtualHost==NULL);
		KTempleteVirtualHost *tvh =  NULL;
		string templete = context->attribute["templete"];
		if(templete.size()>0){
			tvh = vhm->refsTempleteVirtualHost(templete);
			if(tvh==NULL){
				fprintf(stderr,"cann't find vh_templete [%s]\n",templete.c_str());
			}
		}
		KVirtualHost *ov = conf.gvm->refsVirtualHostByName(context->attribute["name"]);
		virtualHost = buildVirtualHost(context->attribute,&vhm->globalVh, tvh,ov);
		if (ov) {
			ov->destroy();
		}
		if (tvh) {
			tvh->destroy();
		}
	} else if (strcasecmp(context->qName.c_str(), "vh_templete") == 0) {
		assert(cur_tvh==NULL);
		string templete = context->attribute["templete"];
		KTempleteVirtualHost *tvh =  NULL;
		if(templete.size()>0){
			tvh = vhm->refsTempleteVirtualHost(templete);
			if(tvh==NULL){
				fprintf(stderr,"cann't find vh_templete [%s]\n",templete.c_str());
			}
		}
		cur_tvh = new KTempleteVirtualHost;
		KAttributeHelper helper(context->attribute);
		if (!buildVirtualHost(&helper,cur_tvh,&vhm->globalVh,  tvh,NULL)) {
			delete cur_tvh;
			cur_tvh = NULL;
		}
		virtualHost = cur_tvh;
		if(tvh){
			tvh->destroy();
		}
	}
	if (cur_tvh) {
		cur_tvh->addEvent(context->qName.c_str(), context->attribute);
	}
	KBaseVirtualHost *bv = virtualHost;
	if (bv == NULL) {
		bv = &vhm->globalVh;	
	}
	std::string parent = context->getParentName();
	if (parent=="vh" || parent=="vhs" || parent=="vh_templete"){
		if (context->qName == "map" || context->qName == "redirect"
			|| context->qName == "default_map") {
			std::string extend = context->attribute["extend"];
			KRedirect *ac = NULL;
			if (strcasecmp(extend.c_str(), "default") != 0) {
				KAcserverManager *am = (c ? c->am : conf.gam);
				ac = am->refsRedirect(extend);
				if (ac == NULL) {
					fprintf(stderr, "cann't find extend [%s]\n",
						context->attribute["extend"].c_str());
					return true;
				}
			}
			if (context->qName == "default_map") {
				KBaseRedirect *rd = new KBaseRedirect(ac, (uint8_t)atoi(context->attribute["confirm_file"].c_str()));
				rd->allowMethod.setMethod(context->attribute["allow_method"].c_str());
				bv->lock.Lock();
				if (bv->defaultRedirect) {
					bv->defaultRedirect->release();
				}
				bv->defaultRedirect = rd;
				bv->lock.Unlock();
			} else {
				std::string value;
				bool file_ext;
				if (context->attribute["file_ext"].size() > 0) {
					file_ext = true;
					value = context->attribute["file_ext"];
				} else if (context->attribute["path"].size() > 0) {
					file_ext = false;
					value = context->attribute["path"];
				}
				if (value.size() == 0) {
					if (ac) {
						ac->release();
					}
					return false;
					//e << "map must have file_ext or path attribute\n";
					//throw e;
				}
				bv->addRedirect(file_ext, value, ac, context->attribute["allow_method"],(uint8_t)atoi(context->attribute["confirm_file"].c_str()), context->attribute["params"]);
			}

		}
		if (strcasecmp(context->qName.c_str(), "index") == 0) {
			bv->addIndexFile(context->attribute["file"],atoi(context->attribute["id"].c_str()));
			return true;
		}
		if (context->qName == "error") {
			bv->addErrorPage(context->attribute.get_int("code"), context->attribute["file"]);
			return true;
		}
		if (strcasecmp(context->qName.c_str(), "alias") == 0) {
			string errMsg;
			if (!bv->addAlias(context->attribute["path"], context->attribute["to"],(virtualHost?virtualHost->doc_root.c_str():conf.path.c_str()), context->attribute["internal"]=="1",0, errMsg)) {
				fprintf(stderr, "%s\n", errMsg.c_str());
			}
			return true;
		}
		if(strcasecmp(context->qName.c_str(), "env") == 0) {
			std::map<string, string>::iterator it;
			for (it = context->attribute.begin(); it != context->attribute.end(); it++) {
				bv->addEnvValue((*it).first.c_str(), (*it).second.c_str());
			}
			return true;
		}
		if (strcasecmp(context->qName.c_str(),"mime_type") == 0) {
			kgl_compress_type compress = kgl_compress_unknow;
			if (!context->attribute["gzip"].empty()) {
				//兼容
				compress = (kgl_compress_type)(context->attribute["gzip"] == "1");
			}
			if (!context->attribute["compress"].empty()) {
				compress = (kgl_compress_type)atoi(context->attribute["compress"].c_str());
			}
			bv->addMimeType(context->attribute["ext"].c_str(), context->attribute["type"].c_str(), compress,atoi(context->attribute["max_age"].c_str()));
		}
	}

	if (virtualHost) {
		for (int i=0;i<2;i++) {
			virtualHost->access[i].startElement(context);
		}
	} else {
		for (int i=0;i<2;i++) {
			this->kaccess[i]->startElement(context);
		}
	}
	return true;
}
void KHttpServerParser::startXml(const std::string &encoding)
{
	//for (int i=0;i<2;i++) {
	//	kaccess[i].startXml(encoding) ;
	//}
}
void KHttpServerParser::endXml(bool result)
{
	//for (int i=0;i<2;i++) {
	//	kaccess[i].endXml(result);
	//}
}
bool KHttpServerParser::startCharacter(KXmlContext *context, char *character,
		int len) {
	if (virtualHost) {
		for (int i=0;i<2;i++) {
			virtualHost->access[i].startCharacter(context,character,len) ;
		}
	} else {
		for (int i=0;i<2;i++) {
			this->kaccess[i]->startCharacter(context,character,len);
		}
	}
	if (virtualHost && context->qName == "host") {
		KSubVirtualHost *svh = new KSubVirtualHost(virtualHost);
		//svh->host = xstrdup(character);
		svh->setHost(character);
		string dir = context->attribute["dir"];
		svh->setDocRoot(virtualHost->doc_root.c_str(),
				(dir.size() > 0 ? dir.c_str() : NULL));
		virtualHost->hosts.push_back(svh);
		return true;
	}
#ifdef ENABLE_BASED_PORT_VH
	if (virtualHost && (context->qName == "port" || context->qName == "bind")) {
		if (*character == '*') {
			return true;
		}
		if (*character == '@' || *character=='#' || *character=='!') {
			virtualHost->binds.push_back(character);
		} else if(isdigit(*character)) {
			KStringBuf s;
			s << "!*:" << character;
			virtualHost->binds.push_back(s.c_str());
		}
		return true;
	}
#endif
	return true;
}
bool KHttpServerParser::endElement(KXmlContext *context) {
	if (virtualHost) {
		for (int i=0;i<2;i++) {
			virtualHost->access[i].endElement(context) ;
		}
	} else {
		for (int i=0;i<2;i++) {
			this->kaccess[i]->endElement(context);
		}
	}
	bool vh_end = false;
	if (strcasecmp(context->qName.c_str(), "vh_templete") == 0) {
		vh_end = true;
	}
	if (strcasecmp(context->qName.c_str(), "vh") == 0) {
		vh_end = true;
	}
	if (vh_end && virtualHost) {
		KConfig *c = (KConfig *)context->getData();
		KVirtualHostManage *vm = (c?c->vm:conf.gvm);
		bool result = false;
		if (cur_tvh) {
			result = vm->updateTempleteVirtualHost(cur_tvh);
		} else {
			if (virtualHost->user_access=="-") {
				//如果是内置的访问控制，则要设置一下已经加载,否则还会从老的复制一份访问控制。
				virtualHost->access_file_loaded = true;
			}
			result = vm->updateVirtualHost(virtualHost);
		}
		if (!result) {
			fprintf(stderr, "vh name=[%s] add failed\n",virtualHost->name.c_str());
			delete virtualHost;
		}
		virtualHost = NULL;
		cur_tvh = NULL;
	}
	return true;
}
bool KHttpServerParser::buildBaseVirtualHost(KXmlAttribute &attribute, KBaseVirtualHost *bv) {
	//兼容老的index 处理
	map<string, string>::iterator it;
	std::vector<std::string> indexFiles;
	std::vector<KBaseString> indexFiles2;
	it = attribute.find("index");
	if (it != attribute.end()) {
		explode((*it).second.c_str(), ',', indexFiles);
		for (size_t i = 0; i < indexFiles.size(); i++) {
			//indexFiles2.push_back(indexFiles[i]);
			bv->addIndexFile(indexFiles[i]);
		}
	}
	map<int, KBaseString> errorPages;
	for (it = attribute.begin(); it != attribute.end(); it++) {
		if (strncasecmp((*it).first.c_str(), "error_", 6) == 0) {
			//errorPages[atoi((*it).first.c_str() + 6)] = (*it).second;
			bv->addErrorPage(atoi((*it).first.c_str() + 6),(*it).second);
		}
	}
	return true;
}
bool KHttpServerParser::buildVirtualHost(KAttributeHelper *ah,
		KVirtualHost *virtualHost, KBaseVirtualHost *gvh,KTempleteVirtualHost *tm,KVirtualHost *ov) {
	KExtendProgramString ds(NULL, virtualHost);
	buildBaseVirtualHost(ah->getAttribute(), virtualHost);
	std::string value;
	if (ah->getValue("envs",value)) {
		virtualHost->parseEnv(value.c_str());
	}
	if (ah->getValue("name", value) && value.size()>0) {
		virtualHost->name = value;
	} else {
		conf.gvm->getAutoName(virtualHost->name);
	}
	value="";
	if (!ah->getValue("doc_root", value) && tm) {
		char *doc_root = ds.parseString(tm->GetDocumentRoot().c_str());
		if (doc_root == NULL) {
			//delete virtualHost;
			return false;
		}
		value = doc_root;
		xfree(doc_root);
	}
	virtualHost->setDocRoot(value);
	if (ah->getValue("browse", value)) {
		if (value == "on") {
			virtualHost->browse = true;
		}
	} else if (tm) {
		virtualHost->browse = tm->browse;
	}
	//处理继承
	bool copyInherit = true;
	int changeInherit = 2;
	if (ah->getValue("inherit", value)) {
		if (strcasecmp(value.c_str(), "off") == 0 || value=="0") {
			virtualHost->inherit = false;
			copyInherit = false;
		} else if (value == "2") {
			changeInherit = 0;
			virtualHost->inherit = false;
		} else {
			virtualHost->inherit = true;
		}
	} else if (tm) {
		virtualHost->inherit = tm->inherit;
	}
	bool ov_clone = false;
	if (ov && ah->getValue("from_web_console",value)) {
		ov_clone = true;
		//来自web管理
		ov->cloneTo(virtualHost, copyInherit, changeInherit);
	}
	if(!ov_clone || !ov->inherit) {
		//两种情况要从全局处理继承设置，一种是无ov,一种是有ov,但ov没继承
		if (!virtualHost->isTemplete()) {
			if (changeInherit>0) {
				//从全局继承，要改变
				changeInherit = 1;
			}
			if (changeInherit==0 || virtualHost->inherit) {
				//复制或继承
				gvh->lock.Lock();
				gvh->cloneTo(virtualHost, copyInherit, changeInherit);
				gvh->lock.Unlock();
			}
		}
	}
#ifdef ENABLE_USER_ACCESS
	if (ah->getValue("access", value)) {
		if (value.size() > 0) {
			virtualHost->setAccess(value);
		}
	} else if (tm) {
		virtualHost->setAccess(tm->user_access);
	}
#endif
	if (ah->getValue("htaccess", value)) {
		if (value.size() > 0) {
			virtualHost->htaccess = value;
		}
	} else if (tm) {
		virtualHost->htaccess = tm->htaccess;
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	std::string certfile;
	std::string keyfile;
	std::string cipher;
	std::string protocols;
#ifdef ENABLE_HTTP2
	virtualHost->alpn = 0;
	if (ah->getValue("alpn", value)) {
		virtualHost->alpn = atoi(value.c_str());
	} else {
		if (ah->getValue("http2", value) && value == "1") {
			KBIT_SET(virtualHost->alpn, KGL_ALPN_HTTP2);
		}
#ifdef ENABLE_HTTP3
		if (ah->getValue("http3", value) && value == "1") {
			KBIT_SET(virtualHost->alpn, KGL_ALPN_HTTP3);
		}
#endif
	}
#endif

	if (ah->getValue("early_data",value)) {
		virtualHost->early_data = atoi(value.c_str()) == 1;
	} else if (tm) {
		virtualHost->early_data = tm->early_data;
	}
	if (!ah->getValue("certificate",certfile) && tm) {
		certfile = tm->cert_file;		
	}
	if(!ah->getValue("certificate_key",keyfile) && tm){
		keyfile = tm->key_file;
	}
	if (!ah->getValue("cipher",cipher) && tm && !tm->cipher.empty()) {
		cipher = tm->cipher;
	}
	if (!ah->getValue("protocols", protocols) && tm && !tm->protocols.empty()) {
		protocols = tm->protocols;
	}
	virtualHost->setSSLInfo(certfile, keyfile, cipher, protocols);
#endif
#ifdef ENABLE_VH_RUN_AS
	string user, group;
	if (!ah->getValue("user", user) && tm) {
		user = tm->user;
	}
#ifdef _WIN32
	if(!ah->getValue("password",group)){
		if(!ah->getValue("group",group) && tm) {
			group = tm->group;
		}
	}
#else
	if (!ah->getValue("group", group) && tm) {
		group = tm->group;
	}
	if (ah->getValue("chroot", value)) {
		if (value == "1" || value == "on") {
			virtualHost->chroot = true;
		}
	} else if (tm) {
		virtualHost->chroot = tm->chroot;
	}
#endif
	virtualHost->setRunAs(user, group);
	if (ah->getValue("app_share", value)) {
		virtualHost->app_share = atoi(value.c_str())==1;
	} else if (tm) {
		virtualHost->app_share = tm->app_share;
	}
	if (ah->getValue("app", value)) {
		virtualHost->setApp(atoi(value.c_str()));
	} else if (tm){
		virtualHost->setApp(tm->app);
	} else {
		virtualHost->setApp(1);
	}
	if (ah->getValue("ip_hash", value)) {
		virtualHost->ip_hash = atoi(value.c_str())==1;
	} else if (tm) {
		virtualHost->ip_hash = tm->ip_hash;
	}

#endif
#ifdef ENABLE_VH_LOG_FILE
	virtualHost->setLogFile(ah, tm);
#endif
#ifdef ENABLE_VH_RS_LIMIT
	if (ah->getValue("speed_limit", value)) {
		virtualHost->setSpeedLimit(value.c_str(),ov);
	} else if (tm) {
		virtualHost->setSpeedLimit(tm->speed_limit,ov);
	}
	if (ah->getValue("max_connect", value)) {
		virtualHost->max_connect = atoi(value.c_str());
	} else if (tm) {
		virtualHost->max_connect = tm->max_connect;
	}
	virtualHost->initConnect(ov);
#endif
	//{{ent
#ifdef ENABLE_BLACK_LIST
	if (ov) {
		assert(ov->blackList);
		virtualHost->blackList = ov->blackList;
		ov->blackList->addRef();
	} else {
		virtualHost->blackList = new KIpList();
	}
#endif//}}
#ifdef ENABLE_VH_FLOW
	if (ah->getValue("fflow", value)) {
		virtualHost->setFlow(value=="1",ov);
	} else if (tm) {
		virtualHost->setFlow(tm->fflow,ov);
	}
#endif
#ifdef ENABLE_VH_QUEUE
	if (ah->getValue("max_worker", value)) {
		virtualHost->max_worker = atoi(value.c_str());
	} else if (tm) {
		virtualHost->max_worker = tm->max_worker;
	}
	if (ah->getValue("max_queue", value)) {
		virtualHost->max_queue = atoi(value.c_str());
	} else if (tm) {
		virtualHost->max_queue = tm->max_queue;
	}
	virtualHost->initQueue(ov);
#endif
	if (ah->getValue("status", value)) {
		virtualHost->SetStatus(atoi(value.c_str()));
	} else if (tm) {
		virtualHost->closed = tm->closed;
	}
	if (virtualHost && tm) {
		std::list<KSubVirtualHost *>::iterator it;
		for (it = tm->hosts.begin(); it != tm->hosts.end(); it++) {
			char *host = NULL;
			if (virtualHost->isTemplete()) {
				host = xstrdup((*it)->host);
			} else {
				host = ds.parseString((*it)->host);
			}
			if (host == NULL) {
				continue;
			}
			KSubVirtualHost *svh = new KSubVirtualHost(virtualHost);
			svh->setHost(host);
			svh->fromTemplete = true;
			svh->setDocRoot(virtualHost->doc_root.c_str(), (*it)->dir);
			virtualHost->hosts.push_back(svh);
		}
		std::list<std::string>::iterator it2;
		for(it2=tm->binds.begin();it2!=tm->binds.end();it2++){
			if(virtualHost->isTemplete()){
				virtualHost->binds.push_back((*it2));
			} else {
				char *bind = ds.parseString((*it2).c_str());
				if (bind) {
					virtualHost->binds.push_back(bind);
					xfree(bind);
				}
			}
		}
		tm->cloneTo(virtualHost, false, 2);
		//合并模板mime类型
		if (tm->mimeType) {
			if (virtualHost->mimeType==NULL) {
				virtualHost->mimeType = new KMimeType;
			}
			tm->mimeType->mergeTo(virtualHost->mimeType,false);
		}
		virtualHost->tvh = tm;
		tm->addRef();
	}
	return true;
}
KVirtualHost *KHttpServerParser::buildVirtualHost(KXmlAttribute&attribute,KBaseVirtualHost *gvh, KTempleteVirtualHost *tm,KVirtualHost *ov) {
	KAttributeHelper helper(attribute);
	return buildVirtualHost(&helper,gvh, tm,ov);
}
KVirtualHost *KHttpServerParser::buildVirtualHost(KAttributeHelper *ah,KBaseVirtualHost *gvh,
		KTempleteVirtualHost *tm,KVirtualHost *ov) {
	KVirtualHost *virtualHost = new KVirtualHost;
	if (buildVirtualHost(ah, virtualHost, gvh,tm,ov)) {
		return virtualHost;
	}
	delete virtualHost;
	return NULL;
}
bool KHttpServerParser::ParseSsl(KVirtualHostManage *vhm, std::map<std::string,std::string> &attribute)
{
	std::string domain = attribute["domain"];
	if (domain.empty()) {
		klog(KLOG_ERR, "set ssl failed domain is empty\n", domain.c_str());
		return false ;
	}
	if (!vhm->BindSsl(domain.c_str(), attribute["certificate"].c_str(), attribute["certificate_key"].c_str())) {
		klog(KLOG_ERR, "set ssl for domain [%s] failed\n", domain.c_str());
		return false;
	}
	return true;
}
#endif
