/*
 * Kconf.gvm->cpp
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 *
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

#include "do_config.h"
#include "KVirtualHostManage.h"
#include "KLineFile.h"
#include "kmalloc.h"
#include "lib.h"
#include "lang.h"
#include "KHttpServerParser.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KTempleteVirtualHost.h"
#include "directory.h"
#include "KHtAccess.h"
#include "KConfigBuilder.h"
#include "KDynamicListen.h"
#include "KHttpFilterManage.h"
#include "WhmContext.h"
using namespace std;

std::string convertInt(int type)
{
	std::stringstream s;
	s << type;
	return s.str();
}
KMutex KVirtualHostManage::lock;
KDynamicListen KVirtualHostManage::dlisten;
volatile int32_t curInstanceId = 1;
void free_ssl_certifycate(void *arg)
{
	KSslCertificate *cert = (KSslCertificate *)arg;
	delete cert;
}
KVirtualHostManage::KVirtualHostManage() {
	//{{ent
#ifdef ENABLE_BLACK_LIST
	globalVh.blackList = new KIpList;
#endif//}}
#ifdef ENABLE_SVH_SSL
	ssl_config = NULL;
#endif
}
KVirtualHostManage::~KVirtualHostManage() {
	std::map<std::string, KVirtualHost *>::iterator it4;
	for (it4 = avh.begin(); it4 != avh.end(); it4++) {
		if (this == conf.gvm) {
			InternalUnBindVirtualHost((*it4).second);
		}
		(*it4).second->destroy();
	}
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	for (it=gtvhs.begin();it!=gtvhs.end();it++) {
		delete (*it).second;
	}
	gtvhs.clear();
	avh.clear();
	kassert(this!=conf.gvm || dlisten.IsEmpty());
#ifdef ENABLE_SVH_SSL
	if (ssl_config) {
		ssl_config->Destroy(free_ssl_certifycate);
	}
#endif
}
void KVirtualHostManage::copy(KVirtualHostManage *vm)
{
	kassert(this == conf.gvm);
	lock.Lock();
	globalVh.swap(&vm->globalVh);
	avh.swap(vm->avh);
	gtvhs.swap(vm->gtvhs);
#ifdef ENABLE_SVH_SSL
	KVirtualHostContainer *tmp_ssl_config = ssl_config;
	ssl_config = vm->ssl_config;
	vm->ssl_config = tmp_ssl_config;
#endif
	InternalBindAllVirtualHost();
	vm->InternalUnbindAllVirtualHost();
	lock.Unlock();
}
kserver *KVirtualHostManage::RefsServer(u_short port)
{
	lock.Lock();
	kserver *server = dlisten.RefsServer(port);
	lock.Unlock();
	return server;
}
void KVirtualHostManage::getAllGroupTemplete(std::list<std::string> &vhs)
{
	lock.Lock();
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	for (it = gtvhs.begin(); it != gtvhs.end(); it++) {
		vhs.push_back((*it).first);
	}
	lock.Unlock();
}
bool KVirtualHostManage::getAllTempleteVh(const char *groupTemplete,std::list<std::string> &vhs) {
	if(groupTemplete==NULL){
		return false;
	}
	bool result = false;
	lock.Lock();
	std::map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(groupTemplete);
	if(it!=gtvhs.end()){
		result = true;
		(*it).second->getAllTemplete(vhs);
	}
	lock.Unlock();
	return result;
}
void KVirtualHostManage::getAllVh(std::list<std::string> &vhs,bool status,bool onlydb) {
	std::stringstream s;
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it = avh.begin(); it != avh.end(); it++) {
		if(!onlydb || (*it).second->db){			
			vhs.push_back((*it).first);
		}
	}
	lock.Unlock();
}
void KVirtualHostManage::getAutoName(std::string &name) {
	std::stringstream s;
	//instancelock.Lock();
	s << kgl_program_start_sec << "_" << katom_inc((void *)&curInstanceId);
	//instancelock.Unlock();
	s.str().swap(name);
}
int KVirtualHostManage::getNextInstanceId() {
	return katom_inc((void *)&curInstanceId);
}
bool KVirtualHostManage::removeTempleteVirtualHost(std::string name)
{
	bool result = false;
	lock.Lock();
	int index = (int)name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		KGTempleteVirtualHost *gtvh = (*it).second;
		if (gtvh) {
			result = gtvh->del(subname.c_str());
		}
		if (gtvh->isEmpty()) {
			gtvhs.erase(it);
			delete gtvh;
		}
	}
	lock.Unlock();
	return result;
}
KTempleteVirtualHost *KVirtualHostManage::refsTempleteVirtualHost(std::string name) {
	KTempleteVirtualHost *tvh = NULL;
	lock.Lock();
	int index = name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		KGTempleteVirtualHost *gtvh = (*it).second;
		if(gtvh){
			tvh = gtvh->findTemplete(subname.c_str(),false);
		}
	}
	lock.Unlock();
	return tvh;
}
bool KVirtualHostManage::updateTempleteVirtualHost(KTempleteVirtualHost *tvh) {
	KTempleteVirtualHost *ov = NULL;
	lock.Lock();
	string name = tvh->name;
	int index = name.find_first_of(':');
	string subname;
	if(index>=0){
		subname = name.substr(index+1);
		name = name.substr(0,index);
	}
	map<std::string, KGTempleteVirtualHost *>::iterator it;
	KGTempleteVirtualHost *gtvh = NULL;
	it = gtvhs.find(name);
	if (it != gtvhs.end()) {
		gtvh = (*it).second;
		if (gtvh) {
			ov = gtvh->findTemplete(subname.c_str(),true);
		}
	}
	if(gtvh == NULL){
		gtvh = new KGTempleteVirtualHost;
		gtvhs.insert(pair<std::string,KGTempleteVirtualHost *>(name,gtvh));
	}
	gtvh->add(subname.c_str(),tvh);
	lock.Unlock();
	if (ov) {
		ov->destroy();
	}
	return true;
}

KVirtualHost *KVirtualHostManage::refsVirtualHostByName(std::string name) {
	KVirtualHost *vh = NULL;
	lock.Lock();
	map<std::string, KVirtualHost *>::iterator it = avh.find(name);
	if (it != avh.end()) {
		vh = (*it).second;
		vh->addRef();
	}
	lock.Unlock();
	return vh;
}
void KVirtualHostManage::Shutdown()
{
	dlisten.Close();
}
void KVirtualHostManage::getMenuHtml(std::stringstream &s,KVirtualHost *v,std::stringstream &url,int t)
{
	KBaseVirtualHost *vh = &globalVh;
	if(v){
		vh = v;
		url << "name=" << v->name << "&";
	}
	s << "<html><head>"
			<< "<LINK href=/main.css type='text/css' rel=stylesheet></head>";
	s << "<body><table border=0><tr><td>";
	s << "[<a href='/vhlist'>" << klang["LANG_VHS"] << "</a>]";
	if (t) {
		s << " ==> [<a href='/vhlist?t=1&id=0'>" << klang["all_tvh"] << "</a>]";
		if (v) {
			url << "t=" << t << "&";
		}
	}
	if (v) {
		s << " ==> " << v->name;	
	}
	s << "</td></tr></table><br>";

	s << "<table width='100%'><tr><td align=left>";
	if (v) {
		s << "[<a href='/vhlist?" << url.str() << "&id=0'>" << klang["detail"] << "</a>] ";
	} else {
		s << "[<a href='/vhlist?id=0'>" << klang["all_vh"] << "</a>] ";// [<a href='/vhlist?t=1&id=0'>" << klang["all_tvh"];
	}	
	s << "[<a href='/vhlist?" << url.str() << "id=1'>" << klang["index"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=2'>" << klang["map_extend"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=3'>" << klang["error_page"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=5'>" << klang["alias"]
			<< "</a>] ";
	s << "[<a href='/vhlist?" << url.str() << "id=8'>" << klang["mime_type"]
			<< "</a>] ";
#ifndef HTTP_PROXY
	if(t==0 && v && v->user_access.size()>0){
		s << "[<a href='/vhlist?" << url.str() << "id=6'>" << klang["lang_requestAccess"] << "</a>]";
		s << "[<a href='/vhlist?" << url.str() << "id=7'>" << klang["lang_responseAccess"] << "</a>]";
	}
#endif
	s << "</td><td align=right>";
	//s << "[<a href=\"javascript:if(confirm('really reload')){ window.location='/reload_vh';}\">" << klang["reload_vh"] << "</a>]";
	s << "</td></tr></table>";
	s << "<hr>";
}
void KVirtualHostManage::getHtml(std::stringstream &s,std::string name, int id,KUrlValue &attribute) {
	int t = atoi(attribute.get("t").c_str());
	stringstream url;
	KBaseVirtualHost *vh = &globalVh;
	KVirtualHost *v = NULL;


	//url << "name=" << name;
	if (name.size() > 0) {
		if (t) {
			v = refsTempleteVirtualHost(name);
		} else {
			v = refsVirtualHostByName(name);
		}
		if (v) {
			vh = v;			
		} else {
			name = "";
		}
	}
	getMenuHtml(s,v,url,t);
	url << "id=" << id;
	if (id==0 && name.size()==0) {
		lock.Lock();
		getAllVhHtml(s,t);
		lock.Unlock();
	} else {
		vh->lock.Lock();
		if (id == 0) {
			getVhDetail(s, (KVirtualHost *) vh,true,t);
		} else if (id == 1) {
			vh->getIndexHtml(url.str(), s);
		} else if (id == 2) {
			vh->getRedirectHtml(url.str(), s);
		} else if (id == 3) {
			vh->getErrorPageHtml(url.str(), s);
		} else if (id == 4) {
			if (v) {
				v->destroy();
			}
			v = refsTempleteVirtualHost(attribute.get("templete").c_str());
			getVhDetail(s, v,false,t);
		} else if (id == 5) {
			vh->getAliasHtml(url.str(), s);
		} else if (id==6) {
			if(v){
				s << v->access[0].htmlAccess(name.c_str());
			}
		} else if (id==7) {
			if(v){
				s << v->access[1].htmlAccess(name.c_str());
			}
		} else if (id==8) {
			vh->getMimeTypeHtml(url.str(),s);
		}
		vh->lock.Unlock();
	}
	if (v) {
		v->destroy();
	}
	s << endTag();
	s << "</body></html>";
}
bool KVirtualHostManage::vhBaseAction(KUrlValue &attribute, std::string &errMsg) {
	string action = attribute["action"];
	string name = attribute["name"];
	int t = atoi(attribute["t"].c_str());
	//string host = attribute["host"];
	KBaseVirtualHost *bvh = &globalVh;
	KVirtualHost *v = NULL;
	bool result = false;
	bool reinherit = true;
	std::map<std::string,std::string> vhdata;
	bool skip_warning = false;
	if (name.size() > 0) {
		if (t) {
			v = refsTempleteVirtualHost(name);
		} else {
			v = refsVirtualHostByName(name);
		}
		bvh = v;
	}
	if (v && v->db) {
		vhdata["vhost"] = name;
	}
	if (action == "vh_add") {
		if (v) {
			errMsg = "name: " ;
			errMsg += name;
			errMsg += " is used!";
		} else {
			KTempleteVirtualHost *tvh = refsTempleteVirtualHost(attribute["templete"]);			
			result = vhAction(NULL, tvh, attribute, errMsg);
			if (tvh) {
				tvh->destroy();
			}
		}
		reinherit = false;
	} else {
		if (name.size() > 0 && v == NULL) {
			errMsg = "cann't find vh";
			return false;
		}
		if (action == "vh_delete") {
			if (v) {
				reinherit = false;
				if (t) {
					result = removeTempleteVirtualHost(name);
				} else {
					removeVirtualHost(v);
					result = true;
				}
				if (v->db || v->ext) {
					errMsg = "Warning! The virtualhost is managed by external file(extend file or database),it will not save to vh.xml file.";
					result = false;
				}
			}	
		} else if (action == "vh_edit") {
			reinherit = false;
			KTempleteVirtualHost *tvh = refsTempleteVirtualHost(attribute["templete"]);
			result = vhAction(v, tvh, attribute, errMsg);
			if (tvh) {
				tvh->destroy();
			}
		} else if (action == "indexadd") {
			attribute["id"] = "1";
			result = bvh->addIndexFile(attribute["index"],atoi(attribute["index_id"].c_str()));
		} else if (action == "indexdelete") {
			attribute["id"] = "1";
			result = bvh->delIndexFile(attribute["index"]);
		} else if (action == "redirectadd") {
			attribute["id"] = "2";
			bool file_ext = false;
			if (attribute["type"] == "file_ext") {
				file_ext = true;
			}
			result = bvh->addRedirect(file_ext, attribute["value"],
					attribute["extend"], attribute["allow_method"],
					(uint8_t)atoi(attribute["confirm_file"].c_str()),attribute["params"]);
		} else if (action == "redirectdelete") {
			attribute["id"] = "2";
			bool file_ext = false;
			if (attribute["type"] == "file_ext") {
				file_ext = true;
			}
			result = bvh->delRedirect(file_ext, attribute["value"]);
		} else if (action == "errorpageadd") {
			attribute["id"] = "3";
			result = bvh->addErrorPage(atoi(attribute["code"].c_str()),
					attribute["url"]);
		} else if (action == "errorpagedelete") {
			attribute["id"] = "3";
			result = bvh->delErrorPage(atoi(attribute["code"].c_str()));
		} else if (action == "aliasadd") {
			attribute["id"] = "5";
			bool internal = false;
			if(attribute["internal"]=="1" || attribute["internal"]=="on"){
				internal = true;
			}
			result = bvh->addAlias(attribute["path"],
				attribute["to"], 
				(v?v->doc_root.c_str():conf.path.c_str()),
				internal,
				atoi(attribute["index"].c_str()), 
				errMsg);
		} else if (action == "aliasdelete") {
			attribute["id"] = "5";
			result = bvh->delAlias(attribute["path"].c_str());
		} else if (action == "mimetypeadd") {
			attribute["id"] = "8";
			kgl_compress_type compress = (kgl_compress_type)atoi(attribute["compress"].c_str());
			int max_age = atoi(attribute["max_age"].c_str());
			bvh->addMimeType(attribute["ext"].c_str(),attribute["type"].c_str(), compress, max_age);
			result = true;
			reinherit = false;
		} else if (action == "mimetypedelete") {
			attribute["id"] = "8";
			result = bvh->delMimeType(attribute["ext"].c_str());
			reinherit = false;
		} else {
			errMsg = "action [" + action + "] is error";
		}
	}
	if (t) {
		reinherit = false;
	}
	if (v) {
		if (v->db || v->ext) {
			if (!skip_warning) {
				errMsg = "Warning! The virtualhost is managed by external file(extend file or database),it will not save to vh.xml file.";
				result = false;
			}
		}
		if (reinherit) {
			inheritVirtualHost(v, true);
		}
		v->destroy();
	} else if (reinherit) {
		inheriteAll();
	}
	return result;
}
void KVirtualHostManage::inheritVirtualHost(KVirtualHost *vh, bool clearFlag) {
	if (vh->isTemplete()) {
		return;
	}
	globalVh.lock.Lock();
	globalVh.inheriTo(vh, clearFlag);
	globalVh.lock.Unlock();
}
void KVirtualHostManage::inheriteAll() {
	std::map<string, KVirtualHost *>::iterator it;
	lock.Lock();
	globalVh.lock.Lock();
	for (it = avh.begin(); it != avh.end(); it++) {
		globalVh.inheriTo((*it).second, true);
	}
	globalVh.lock.Unlock();
	lock.Unlock();
}
bool KVirtualHostManage::vhAction(KVirtualHost *ov,KTempleteVirtualHost *tm,
		KUrlValue &attribute, std::string &errMsg) {
	attribute["from_web_console"] = 1;
	KAttributeHelper ah(attribute.get());
	bool isTemplate = atoi(attribute["t"].c_str())>0;
	KTempleteVirtualHost *tvh = NULL;
	KVirtualHost *vh ;
	if (isTemplate) {
		tvh = new KTempleteVirtualHost;
		tvh->initEvents = attribute["init_event"];
		tvh->destroyEvents = attribute["destroy_event"];
		tvh->updateEvents = attribute["update_event"];
		vh = tvh;
	} else {
		vh = new KVirtualHost;
	}
#ifndef HTTP_PROXY
	if(!KHttpServerParser::buildVirtualHost(&ah,vh,&conf.gvm->globalVh,tm,ov)){
		//todo:error
	}
#endif
	if (vh->name.size()==0) {
		errMsg = "name cann't be empty";
		delete vh;
		return false;
	}
	KLineFile lf;
	lf.init(attribute["host"].c_str());
	for (;;) {
		bool addFlag = true;
		char *line = lf.readLine();
		if (line == NULL) {
			break;
		}	
		KSubVirtualHost *svh = new KSubVirtualHost(vh);
		svh->setHost(line);
		svh->setDocRoot(vh->doc_root.c_str(), NULL);
		vh->hosts.push_back(svh);		
	}
#ifdef ENABLE_BASED_PORT_VH
	lf.init(attribute["bind"].c_str());
	for (;;) {
		char *line = lf.readLine();
		if (line == NULL) {
			break;
		}
		if (*line=='*') {
			continue;
		}
		if (*line=='@' || *line=='#' || *line=='!') {
			vh->binds.push_back(line);
			continue;
		}
		if (isdigit(*line)) {
			KStringBuf s;
			s << "!*:" << line;
			vh->binds.push_back(s.getString());
		}
	}
#endif
	if (ov) {
		vh->db = ov->db;
		vh->ext = ov->ext;
	} else {
		vh->db = false;
		vh->ext = false;
	}
	bool result;
	if (tvh) {
		result = updateTempleteVirtualHost(tvh);
	} else {
		result = updateVirtualHost(vh, ov);
	}
	return result;
}
bool KVirtualHostManage::saveConfig(std::string &errMsg) {
	return KConfigBuilder::saveConfig();
}
void KVirtualHostManage::build(stringstream &s) {
	//s << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
	lock.Lock();
	s << "<vhs ";
	globalVh.buildBaseXML(s);
	s << "</vhs>\n";
	std::map<std::string, KGTempleteVirtualHost *>::iterator it2;
	for (it2=gtvhs.begin();it2!=gtvhs.end();it2++) {
		std::map<string, KTempleteVirtualHost *>::iterator it3;
		for(it3=(*it2).second->tvhs.begin();it3!=(*it2).second->tvhs.end();it3++){
			if ((*it3).second->ext || (*it3).second->db) {
				continue;
			}
			s << "<vh_templete ";
			(*it3).second->buildXML(s);
			s << "</vh_templete>\n";
		}
	}
	std::map<string, KVirtualHost *>::iterator it;
	s << "\t<!--vh start-->\n";
	for (it = avh.begin(); it != avh.end(); it++) {
		if ((*it).second->ext || (*it).second->db) {
			continue;
		}
		s << "<vh ";
		(*it).second->buildXML(s);
		s << "</vh>\n";
	}
	s << "\t<!--vh end-->\n";
	lock.Unlock();
}
bool KVirtualHostManage::updateVirtualHost(KVirtualHost *vh,KVirtualHost *ov)
{
	lock.Lock();
	if (ov) {
#ifdef ENABLE_VH_RUN_AS
		ov->need_kill_process = vh->caculateNeedKillProcess(ov);
#endif
		if (ov && this == conf.gvm) {
			internalRemoveVirtualHost(ov);
		}
	}
	if (vh->name.empty()) {
		getAutoName(vh->name);
	}
	bool result = internalAddVirtualHost(vh,ov);
	if (ov && this == conf.gvm) {
		InternalUnBindVirtualHost(ov);
	}
	lock.Unlock();
	return result;
}
bool KVirtualHostManage::updateVirtualHost(KVirtualHost *vh) {

	KVirtualHost *ov = refsVirtualHostByName(vh->name);
	bool result = updateVirtualHost(vh,ov);
	if (ov) {
		ov->destroy();
	}
	return result;
}
/*
 * 增加虚拟主机
 */
bool KVirtualHostManage::addVirtualHost(KVirtualHost *vh) {
	if (vh->name.size() == 0) {
		getAutoName(vh->name);
	}
	lock.Lock();
	bool result = internalAddVirtualHost(vh,NULL);
	lock.Unlock();
	return result;
}
/*
 * 删除虚拟主机
 */
bool KVirtualHostManage::removeVirtualHost(KVirtualHost *vh) {
	lock.Lock();
#ifdef ENABLE_VH_RUN_AS
	vh->need_kill_process = 1;
#endif
	bool result = internalRemoveVirtualHost(vh);
	if (this == conf.gvm) {
		InternalUnBindVirtualHost(vh);
	}
	lock.Unlock();
	return result;
}
bool KVirtualHostManage::internalAddVirtualHost(KVirtualHost *vh,KVirtualHost *ov) {
#if 0
	if (vh->empty()) {
		klog(KLOG_WARNING, "skip add empty virtualhost [%s].\n", vh->name.c_str());
		return false;
	}
#endif
#ifdef ENABLE_USER_ACCESS
	vh->loadAccess(ov);
#endif
	std::map<std::string, KVirtualHost *>::iterator it;
	it = avh.find(vh->name);
	if (it != avh.end()) {
		klog(KLOG_ERR,"Cann't add VirtualHost [%s] name duplicate.\n",vh->name.c_str());
		return false;
	}
	if (this==conf.gvm) {
		InternalBindVirtualHost(vh);
	}
	avh.insert(pair<string, KVirtualHost *> (vh->name, vh));
	vh->addRef();
	return true;
}
void KVirtualHostManage::UnBindGlobalVirtualHost(kserver *server)
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it = avh.begin(); it != avh.end(); it++) {
		kserver_remove_global_vh(server,(*it).second);
	}
}
void KVirtualHostManage::BindGlobalVirtualHost(kserver *server)
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		kserver_add_global_vh(server,(*it).second);
	}
}
bool KVirtualHostManage::internalRemoveVirtualHost(KVirtualHost *vh) {
	
	std::map<std::string, KVirtualHost *>::iterator it;
	it = avh.find(vh->name);
	if (it == avh.end() || (*it).second!=vh) {
		return false;
	}
	avh.erase(it);
	kassert(this == conf.gvm);
	vh->destroy();
	return true;
}
int KVirtualHostManage::find_domain(const char *site, WhmContext *ctx)
{
	int site_len = strlen(site);
	const char *p = strchr(site, ':');
	int port = 0;
	if (p) {
		port = atoi(p + 1);
		site_len = p - site;
	}
	unsigned char bind_site[256];
	if (!revert_hostname(site, site_len, bind_site, sizeof(bind_site))) {
		return WHM_CALL_FAILED;
	}
	
	lock.Lock();
	dlisten.QueryDomain(bind_site, port, ctx);
	lock.Unlock();
	return WHM_OK;
}
/*
 * 查找虚拟主机并绑定在rq上。
 */
query_vh_result KVirtualHostManage::queryVirtualHost(kserver *ls,KSubVirtualHost **rq_svh,const char *site,int site_len) {
	assert(ls);
	query_vh_result result = query_vh_host_not_found;
	if (site_len==0) {
		site_len = strlen(site);
	}
	unsigned char bind_site[256];
	if (!revert_hostname(site,site_len,bind_site,sizeof(bind_site))) {
		return result;
	}
	KVirtualHostContainer* vhc = (KVirtualHostContainer*)kserver_get_opaque(ls);
	if (vhc == NULL) {
		return result;
	}
	lock.Lock();
	KSubVirtualHost *svh = (KSubVirtualHost *)vhc->find(bind_site);
	if (svh) {
		result = query_vh_success;
		if ((*rq_svh) != svh) {
			//虚拟主机变化了?把老的释放，引用新的
			if (*rq_svh) {
				(*rq_svh)->release();
				(*rq_svh) = NULL;
			}
			svh->vh->addRef();
			(*rq_svh) = svh;
		}
	}
	lock.Unlock();
	return result;
}

void KVirtualHostManage::getVhDetail(std::stringstream &s, KVirtualHost *vh,bool edit,int t) {
	//	string host;
	string action = "vh_add";
	if (edit) {
		action = "vh_edit";
	}
	string name;
	if (vh) {
		name = vh->name;
	}
	s << "<form name='frm' action='/vhbase?action=" << action << "&t=" << t;
	if (vh) {
		if(!edit) {
			//vh is a templete
			s << "&templete=" << vh->name;
		} else if(vh->tvh){
			s << "&templete=" << vh->tvh->name;
		}
	}
	s << "' method='post'>";
	s << "<table border=1>";
	s << "<tr><td>" << LANG_NAME << "</td>";
	s << "<td>";
	s << "<input name='name' value='" ;
	if (edit) {
		if (vh) {
			s << vh->name.c_str();
		}
		s << "' readonly";
	} else {
		s << "'";
	}
	s << ">";	
	s << klang["status"] << ":<input name='status' size=3 value='" << (vh?vh->closed:0) << "'>";
	s << "</td></tr>";
	s << "<tr><td>" << klang["doc_root"] << "</td><td><input name='doc_root' size=30 value='"
			<< (vh ? vh->GetDocumentRoot() : "") << "'>";
#ifndef _WIN32
	s << "<input name='chroot' type='checkbox' value='1'" << ((vh && vh->chroot) ? "checked" : "") << ">chroot";
#endif
#ifdef ENABLE_BASED_PORT_VH
	s << "<tr><td>" << klang["bind"] << "</td>";
	s << "<td><textarea name='bind' rows='3' cols='25'>";
	if (vh && edit) {
		list<string>::iterator it2;
		for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
			s << (*it2) << "\n";
		}
	}
	s << "</textarea></td></tr>\n";
#endif
	s << "<tr><td>" << klang["vh_host"] << "</td>";
	s << "<td><textarea name='host' rows='4' cols='25'>";
	if (vh && edit) {
		list<KSubVirtualHost *>::iterator it;
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			if ((*it)->fromTemplete) {
				continue;
			}
			if ((*it)->wide) {
				s << "*";
			}
			s << (*it)->host;
			if (strcmp((*it)->dir, "/") != 0
#ifdef ENABLE_SVH_SSL
				|| (*it)->ssl_param!=NULL
#endif
			){
				s << "|" << (*it)->dir;
#ifdef ENABLE_SVH_SSL
				if ((*it)->ssl_param != NULL) {
					s << KGL_SSL_PARAM_SPLIT_CHAR << (*it)->ssl_param;
				}
#endif
			}
			s << "\n";
		}
	}
	s << "</textarea></td></tr>\n";

	s << "</td></tr>\n";
	s << "<tr><td>" << klang["inherit"]
			<< "</td><td><input name='inherit' type='radio' value='1' ";
	if (vh == NULL || vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["inherit"]
			<< "<input name='inherit' type='radio' value='0' ";
	if (vh && !vh->inherit) {
		s << "checked";
	}
	s << ">" << klang["no_inherit"]
			<< "<input name='inherit' type='radio' value='2'>";
	s << klang["no_inherit2"] << "</td></tr>\n";
#ifdef ENABLE_VH_RUN_AS
	s << "<tr><td>" << LANG_RUN_USER << "</td><td>" << LANG_USER
			<< ":<input name='user' value='";
	s << (vh ? vh->user : "") << "' autocomplete='off' size=10> ";
#ifdef _WIN32
	s << LANG_PASS;
#else
	s << klang["group"];
#endif
	s << ":<input name='group' "
#ifdef _WIN32
			<< "type='password' "
#endif
			<< "value='"
#ifndef _WIN32
			<< (vh ? vh->group : "")
#endif
			<< "' autocomplete='off' size=10><td></tr>\n";
#endif
#ifdef ENABLE_VH_LOG_FILE
	s << "<tr><td>" << klang["log_file"]
			<< "</td><td><input name='log_file' value='" << (vh ? vh->logFile
			: "") << "'></td></tr>\n";
	s << "<tr><td>" << klang["log_mkdir"]
			<< "</td><td><input name='log_mkdir' type='radio' value='on' ";
	bool mkdirFlag = false;
	if (vh && vh->logger && vh->logger->mkdirFlag) {
		mkdirFlag = true;
	}
	if (mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_mkdir' type='radio' value='off' ";
	if (!mkdirFlag) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";

	//log_handle
	s << "<tr><td>" << klang["log_handle"]
			<< "</td><td><input name='log_handle' type='radio' value='on' ";
	bool log_handle = true;
	if (vh && vh->logger && !vh->logger->log_handle) {
		log_handle = false;
	}
	if (log_handle) {
		s << "checked";
	}
	s << ">" << LANG_ON << "<input name='log_handle' type='radio' value='off' ";
	if (!log_handle) {
		s << "checked";
	}
	s << ">" << LANG_OFF << "</td></tr>\n";


	string rotateTime;
	if (vh && vh->logger) {
		vh->logger->getRotateTime(rotateTime);
	}
	s << "<tr><td>" << LANG_LOG_ROTATE_TIME
			<< "</td><td><input name='log_rotate_time' value='" << rotateTime
			<< "'></td></tr>\n";
	s << "<td>" << klang["log_rotate_size"]
			<< "</td><td><input name='log_rotate_size' value='" << (vh
			&& vh->logger ? vh->logger->rotateSize : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_day"]
			<< "</td><td><input name='logs_day' value='" << (vh
			&& vh->logger ? vh->logger->logs_day : 0) << "'></td></tr>\n";
	s << "<td>" << klang["logs_size"]
			<< "</td><td><input name='logs_size' value='" << (vh
			&& vh->logger ? vh->logger->logs_size : 0) << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["option"]
			<< "</td><td>";
	s << "<input name='browse' type='checkbox' value='on'"
			<< ((vh && vh->browse) ? "checked" : "") << ">" << klang["browse"];	

#ifdef ENABLE_VH_FLOW
	s << "<input name='fflow' type='checkbox' value='1'"
			<< ((vh && vh->fflow) ? "checked" : "") << ">" << klang["flow"];
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#ifdef ENABLE_HTTP2
	s << "<input name='http2' type='checkbox' value='1'"
		<< ((vh && vh->http2) ? "checked" : "") << ">http2";
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
	s << "<input type='checkbox' name='early_data' value='1' ";
	if (vh && vh->early_data) {
		s << "checked";
	}
	s << ">early_data";
#endif
#endif
	s << "</td></tr>\n";
#ifdef ENABLE_USER_ACCESS
	s << "<tr><td>" << klang["access_file"]
		<< "</td><td><input name='access' value='" 
		<< (vh ? vh->user_access : "") << "'></td></tr>\n";
#endif
	s << "<tr><td>" << klang["htaccess"]
		<< "</td><td><input name='htaccess' value='"
		<< (vh ? vh->htaccess : "") << "'></td></tr>\n";
	s <<  "<tr><td>" << klang["app_count"] << "</td><td>";
	s << "<input name='app' value='" << (vh ? vh->app : 1) << "' size='4'>";
	s << "<input type='checkbox' name='ip_hash' value='1' " ;
	if(vh && vh->ip_hash){
		s << "checked";
	}
	s << ">" << klang["ip_hash"] << "</td></tr>";
	s << "<tr><td>" << klang["app_share"] << "</td><td>";
	s << "<input type='radio' name='app_share' value='0' ";
	if(vh && vh->app_share==0){
		s << "checked";
	}
	s << "/>" << klang["app_share0"];
	s << "<input type='radio' name='app_share' value='1' ";
	if (vh==NULL || vh->app_share==1) {
		s << "checked";
	}
	s << "/>" << klang["app_share1"];
#if 0
	s << "<input type='radio' name='app_share' value='2' ";
	if(vh && vh->app_share==2){
		s << "checked";
	}
	s << "/>" << klang["app_share2"];
	s << "<input type='radio' name='app_share' value='3' ";
	if(vh && vh->app_share==3){
		s << "checked";
	}
	s << "/>" << klang["app_share3"];
#endif
	s << "</td></tr>\n";
#ifdef ENABLE_VH_RS_LIMIT
	s << "<tr><td>" << klang["connect"]
			<< "</td><td><input name='max_connect' value='";
	s << (vh ? vh->max_connect : 0) << "'></td></tr>\n";
	s << "<tr><td>" << LANG_LIMIT_SPEED
			<< "</td><td><input name='speed_limit' value='";
	s << (vh ? vh->speed_limit : 0) << "'></td></tr>\n";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<tr><td>" << klang["max_worker"]
			<< "</td><td><input name='max_worker' value='";
	s << (vh ? vh->max_worker : 0) << "'></td></tr>\n";

	s << "<tr><td>" << klang["max_queue"]
			<< "</td><td><input name='max_queue' value='";
	s << (vh ? vh->max_queue : 0) << "'></td></tr>\n";
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	s << "<tr><td>" << klang["cert_file"] << "</td><td>";
	s << "<input name='certificate' value='";
	if (vh) {
		s << vh->cert_file;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>" << klang["private_file"] << "</td><td>";
	s << "<input name='certificate_key' value='";
	if (vh) {
		s << vh->key_file;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>cipher</td><td>";
	s << "<input name='cipher' value='";
	if (vh) {
		s << vh->cipher;
	}
	s << "'></td></tr>\n";
	s << "<tr><td>protocols</td><td>";
	s << "<input name='protocols' value='";
	if (vh) {
		s << vh->protocols;
	}
	s << "'></td></tr>\n";
#endif
	s << "<tr><td>envs</td><td>";
	s << "<input name='envs' size=32 value=\"";
	if (vh) {
		vh->buildEnv(s);
	}
	s << "\"></td></tr>\n";
	if(t || (vh && vh->isTemplete() && edit)){
		KTempleteVirtualHost *tvh = NULL;
		if(vh){
			tvh = static_cast<KTempleteVirtualHost *>(vh);
		}
		s << "<tr><td>init event:</td>";
		s << "<td><input name='init_event' size=32 value='" << (tvh?tvh->initEvents:"") << "'></td></tr>";
		s << "<tr><td>destroy event:</td>";
		s << "<td><input name='destroy_event' size=32 value='" << (tvh?tvh->destroyEvents:"") << "'></td></tr>";
		s << "<tr><td>update event:</td>";
		s << "<td><input name='update_event' size=32 value='" << (tvh?tvh->updateEvents:"") << "'></td></tr>";

	}
	s << "</table>\n";
	s << "<input type=submit value='" << LANG_SUBMIT << "'>";
	s << "</form>";
}
void KVirtualHostManage::getVhIndex(std::stringstream &s,KVirtualHost *vh,int id,int t)
{
		vh->lock.Lock();
		s << "<tr id='tr" << id << "' style='background-color: #ffffff' onmouseover=\"setbgcolor('tr";
		s << id << "','#bbbbbb')\" onmouseout=\"setbgcolor('tr" << id << "','#ffffff')\">";
		s << "<td>";
		s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/vhbase?";
		s << "name=" << vh->name << "&action=vh_delete&t=" << t << "';}\">"	<< LANG_DELETE << "</a>]";
		if (t) {
			s << "[<a href='/vhlist?id=4&templete=" << vh->name << "'>" << klang["new_vh"] << "</a>]";
		}
		s << "</td><td ";
		if(vh->closed){
			s << "bgcolor='#bbbbbb'";
		}
		s << "><a href='/vhlist?id=0&name=" << vh->name << "&t=" << t << "'";
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		if (vh->ssl_ctx) {
			s << " class=\"ssl_enabled\"";
		}
#endif
		s << ">";
		s << vh->name;
		s << "</td><td>";
		list<KSubVirtualHost *>::iterator it2;
		for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
			s << "<a href=# title=\"";
			if (strcmp((*it2)->dir, "/") != 0
				//{{ent
#ifdef ENABLE_SVH_SSL
				|| (*it2)->ssl_param != NULL
#endif//}}
				) {
				s << (*it2)->dir;
				//{{ent
#ifdef ENABLE_SVH_SSL
				if ((*it2)->ssl_param != NULL) {
					s << KGL_SSL_PARAM_SPLIT_CHAR << (*it2)->ssl_param;
				}
#endif//}}
			}
			s << "\"";
#ifdef ENABLE_SVH_SSL
			if ((*it2)->ssl_ctx) {
				s << " class=\"ssl_enabled\"";
			}
#endif
			s << ">";
			if ((*it2)->wide) {
				s << "*";
			}
			s << (*it2)->host;
			s << "</a> ";
		}
		s << "</td><td ><div title='" << vh->doc_root << "'>" << vh->GetDocumentRoot() << "</div></td>";
#ifdef ENABLE_VH_RUN_AS
		s << "<td >" << (vh->user.size() > 0 ? vh->user : "&nbsp;");
#ifndef _WIN32
		if (vh->group.size() > 0) {
			s << ":" << vh->group;
		}
#endif
		s << "</td>";
#endif
		s << "<td >" << (vh->inherit ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_VH_LOG_FILE
		s << "<td >" << (!vh->logFile.empty() ? vh->logFile : "&nbsp;") << "</td>";
#endif
		s << "<td >" << (vh->browse ? LANG_ON : LANG_OFF) << "</td>";
#ifdef ENABLE_USER_ACCESS
		s << "<td >" << (!vh->user_access.empty() ? vh->user_access : "&nbsp;") << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
		s << "<td >" << vh->GetConnectionCount() << "/" 	<< vh->max_connect << "</td>";
		s << "<td >" ;
#ifdef ENABLE_VH_FLOW
		s << vh->get_speed(false) << "/";
#endif
		s << vh->speed_limit << "</td>";
#endif		
#ifdef ENABLE_VH_FLOW
		s << "<td>";
		if (vh->flow) {
			s << vh->flow->flow << " " << (vh->flow->flow>0?(vh->flow->cache * 100) / vh->flow->flow :0)<< "%";
		}
		s << "</td>";
#endif
#ifdef ENABLE_VH_QUEUE
		s << "<td >";
		if (vh->queue) {
			s << vh->queue->getWorkerCount() << "/" << vh->queue->getMaxWorker();
		}
		s << "</td>";
		s << "<td >";
		if (vh->queue) {
			s << vh->queue->getQueueSize() << "/" << vh->queue->getMaxQueue();
		}
		s << "</td>";
#endif
		//*
		s << "<td >";
		if (vh->tvh) {
			s << "<a href='/vhlist?id=0&name=" << vh->tvh->name << "&t=1'>" << vh->tvh->name << "</a>";
		} else {
			s << "&nbsp;";
		}
		s << "</td>";
		//*/
		s << "</tr>\n";
		vh->lock.Unlock();
}
#ifdef ENABLE_VH_FLOW
void KVirtualHostManage::dumpLoad(KVirtualHostEvent *ctx,bool revers,const char *prefix,int prefix_len) {
	KStringBuf s2;
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		KVirtualHost *vh = (*it).second;
		if (prefix && revers == (strncmp(vh->name.c_str(),prefix,prefix_len)==0)) {
			continue;			
		}
		int connect_count = vh->GetConnectionCount();
		INT64 speed = vh->get_speed(true);
		if (connect_count==0 && speed==0) {
			continue;
		}
		s2 << vh->name.c_str() << "\t";
		s2 << connect_count << "\t";
		s2 << speed << "\t";
#ifdef ENABLE_VH_QUEUE
		if (vh->queue) {
			s2 << vh->queue->getQueueSize() << "\t";
			s2 << vh->queue->getWorkerCount() << "\t";
		} else
#endif
			s2 << "0\t0";
		s2 << "\n";
	}
	lock.Unlock();
	ctx->add("load",s2.getString());
}
//extend=4,导出上下行，及缓存流量
void KVirtualHostManage::dumpFlow(KVirtualHostEvent *ctx,bool revers,const char *prefix,int prefix_len,int extend)
{
	char buf[64];
	KStringBuf s;
	KStringBuf s2;
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		KVirtualHost *vh = (*it).second;		
		if (!vh->fflow || vh->flow==NULL || vh->flow->flow==0) {
			continue;
		}
		if (prefix && revers == (strncmp(vh->name.c_str(),prefix,prefix_len)==0)) {
			continue;			
		}
		int64_t post_flow;
		int len = vh->flow->dump(buf,sizeof(buf),post_flow);
		s << vh->name.c_str() << "\t";
		if (len>0) {
			s.write_all(buf,len);
		}
		if (extend == 4) {
			s << "\t" << (INT64)post_flow;
		}
		//{{ent
#ifdef ENABLE_BLACK_LIST
		if (vh->blackList && extend>=3) {
			INT64 total_error_upstream, total_request, total_upstream;
			vh->blackList->getStat(total_request, total_error_upstream,total_upstream, true);
			s << "\t" << total_error_upstream << "\t" << total_upstream << "\t" << total_request;
		}
#endif//}}
		s << "\n";
	}
	//{{ent
#ifdef ENABLE_BLACK_LIST
	if (extend>0 && extend<3) {
		for (it=avh.begin();it!=avh.end();it++) {
			KVirtualHost *vh = (*it).second;
			if (prefix && revers == (strncmp(vh->name.c_str(),prefix,prefix_len)==0)) {
				continue;
			}
			s2 << vh->name.c_str() << "\t";
			s2 << vh->GetConnectionCount() << "\t";
			s2 << vh->get_speed(extend==2) << "\t";
#ifdef ENABLE_VH_QUEUE
			if (vh->queue) {
				s2 << vh->queue->getQueueSize() << "\t";
				s2 << vh->queue->getWorkerCount() << "\t";
			} else
#endif
				s2 << "0\t0\t";
			if (vh->blackList) {
				INT64  total_error_upstream,total_request,total_upstream;
				vh->blackList->getStat(total_request, total_error_upstream,total_upstream, extend==2);
				s2 << "\t" << total_error_upstream << "\t" << total_upstream << "\t" << total_request;
			}
			s2 << "\n";
		}
	}
#endif//}}
	lock.Unlock();
	ctx->add("flow",s.getString());
	if (extend>0 && s2.getSize()>0) {
		ctx->add("stat",s2.getString());
	}
}
void KVirtualHostManage::dumpFlow()
{
#ifdef _WIN32
    const char *formatString="%s\t%I64d\t%I64d\n";
#else
    const char *formatString = "%s\t%lld\t%lld";
#endif
	FILE *fp = NULL;
	std::string flow_file = conf.path;
	flow_file += "etc/flow.log";
	lock.Lock();
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		//todo: dump vh flow
		KVirtualHost *vh = (*it).second;
		if (!vh->fflow || vh->flow==NULL) {
			continue;
		}		
		if (fp==NULL) {
			fp = fopen(flow_file.c_str(),"a+");
			if (fp) {
				fprintf(fp,"#flow auto writed\n");
			}
		}
		if (fp) {
			fprintf(fp,formatString,vh->name.c_str(),vh->flow->flow,vh->flow->cache);
		}		
	}
	lock.Unlock();
	if (fp) {
		fclose(fp);
	}
}
#endif
void KVirtualHostManage::getAllVhHtml(std::stringstream &s,int t) {
	map<string, KVirtualHost *>::iterator it;
	s << "<script language='javascript'>\r\n"
		"	function setbgcolor(id,color){"
		"		document.getElementById(id).style.backgroundColor = color;"
		"	}"
		"</script>\r\n";
	s << "[<a href='/vhlist?id=4&t=" << t << "'>" << (t?klang["new_tvh"]:klang["new_vh"]) << "</a>] ";
	if (!t) {
		s << avh.size();
	}
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"	<< LANG_NAME << "</td>";
	s << "<td>" << klang["vh_host"] << "</td><td>" << klang["doc_root"]	<< "</td>";
#ifdef ENABLE_VH_RUN_AS
	s << "<td>" << LANG_RUN_USER << "</td>";
#endif
	s << "<td>" << klang["inherit"] << "</td>";
#ifdef ENABLE_VH_LOG_FILE
	s << "<td>" << klang["log_file"] << "</td>";
#endif
	s << "<td>" << klang["browse"] << "</td>";
#ifdef ENABLE_USER_ACCESS
	s << "<td>" << klang["access_file"] << "</td>";
#endif
#ifdef ENABLE_VH_RS_LIMIT
	s << "<td>" << klang["connect"] << "/" << klang["limit"] << "</td>";
	s << "<td>" << klang["speed"] << "/" << klang["limit"] << "</td>";
#endif
#ifdef ENABLE_VH_FLOW
	s << "<td>" << klang["flow"] << "</td>";
#endif
#ifdef ENABLE_VH_QUEUE
	s << "<td>" << klang["worker"] << "</td>";
	s << "<td>" << klang["queue"]  << "</td>";
#endif
	s << "<td>" << klang["templete"] << "</td>";
	s << "</tr>";
	int id=0;
	if (t==1) {
		//vh templete
		std::map<std::string, KGTempleteVirtualHost *>::iterator it2;
		for(it2=gtvhs.begin();it2!=gtvhs.end();it2++){
			std::map<std::string,KTempleteVirtualHost *>::iterator it3;
			for(it3=(*it2).second->tvhs.begin();it3!=(*it2).second->tvhs.end();it3++){
				getVhIndex(s,(*it3).second,id,t);
				id++;
			}
		}
	} else {
		//vh
		for (it = avh.begin(); it != avh.end(); it++,id++) {
			getVhIndex(s,(*it).second,id,t);
		}
	}
	s << "</table>";
	s << "[<a href='/vhlist?id=4&t=" << t << "'>" << (t?klang["new_tvh"]:klang["new_vh"])  << "</a>]";
}
void KVirtualHostManage::BindGlobalListen(KListenHost *listen)
{
	lock.Lock();
	dlisten.AddGlobal(listen);
	lock.Unlock();
}
void KVirtualHostManage::UnBindGlobalListen(KListenHost *listen)
{
	lock.Lock();
	dlisten.RemoveGlobal(listen);
	lock.Unlock();
}
void KVirtualHostManage::UnBindGlobalListens(std::vector<KListenHost *> &services)
{
	lock.Lock();
	for (size_t i = 0; i < services.size(); i++) {
		dlisten.RemoveGlobal(services[i]);
	}
	lock.Unlock();
	return;
}
void KVirtualHostManage::BindGlobalListens(std::vector<KListenHost *> &services)
{
	lock.Lock();
	for (size_t i=0;i<services.size();i++) {
		//防止加载时间太长，而安全进程误认为挂掉。
		setActive();
		dlisten.AddGlobal(services[i]);
	}
	lock.Unlock();
	return;
}
int KVirtualHostManage::getCount()
{
	lock.Lock();
	int count = avh.size();
	lock.Unlock();
	return count;
}
void KVirtualHostManage::GetListenWhm(WhmContext *ctx)
{
	lock.Lock();
	dlisten.GetListenWhm(ctx);
	lock.Unlock();
	return;
}
void KVirtualHostManage::GetListenHtml(std::stringstream &s)
{
	lock.Lock();
	dlisten.GetListenHtml(s);
	lock.Unlock();
}
void KVirtualHostManage::InternalUnBindVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_VH_RUN_AS
	if (vh->need_kill_process) {
		vh->KillAllProcess();
	}
#endif
	if (vh->binds.empty()) {
		dlisten.UnbindGlobalVirtualHost(vh);
		return;
	}
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it2;
	for (it2 = vh->binds.begin(); it2 != vh->binds.end(); it2++) {
		const char *bind = (*it2).c_str();
		if (*bind == '!') {
			dlisten.RemoveDynamic(bind + 1, vh);
		}
	}
#endif
}
void KVirtualHostManage::InternalBindVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_SVH_SSL
	if (ssl_config) {
		std::list<KSubVirtualHost *>::iterator it2;
		for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
			KSubVirtualHost *svh = (*it2);
			if (svh->ssl_ctx != NULL) {
				continue;
			}
			KSslCertificate *cert = (KSslCertificate *)ssl_config->find(svh->bind_host, svh->wide);
			if (cert) {
				svh->SetSslInfo(cert->cert_file.c_str(), cert->key_file.c_str());
			}
		}
	}
#endif
	if (vh->binds.empty()) {
		dlisten.BindGlobalVirtualHost(vh);
		return;
	}
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string>::iterator it;
	for (it =vh->binds.begin(); it !=vh->binds.end();it++) {
		const char *bind = (*it).c_str();
		if (*bind=='!') {
			dlisten.AddDynamic(bind+1,vh);
		}
	}
#endif
}
void KVirtualHostManage::InternalUnbindAllVirtualHost()
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it = avh.begin(); it != avh.end(); it++) {
		InternalUnBindVirtualHost((*it).second);
	}
}
void KVirtualHostManage::InternalBindAllVirtualHost()
{
	std::map<std::string, KVirtualHost *>::iterator it;
	for (it=avh.begin();it!=avh.end();it++) {
		InternalBindVirtualHost((*it).second);
	}
}
bool KVirtualHostManage::BindSsl(const char *domain, const char *cert_file, const char *key_file)
{
#ifdef ENABLE_SVH_SSL
	KSslCertificate *cert = new KSslCertificate;
	cert->cert_file = cert_file;
	cert->key_file = key_file;
	lock.Lock();
	if (ssl_config==NULL) {
		ssl_config = new KVirtualHostContainer;
	}
	bool result = ssl_config->bind(domain, cert, kgl_bind_unique);
	if (!result) {
		delete cert;
	}
	lock.Unlock();
#endif
	return true;
}

