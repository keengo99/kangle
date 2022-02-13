#include <vector>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "KBaseVirtualHost.h"
#include "lang.h"
#include "kmalloc.h"
#include "KAcserverManager.h"
#include "KVirtualHost.h"
#include "KApiPipeStream.h"
#include "KApiRedirect.h"
#include "lang.h"
#include "http.h"
#include "KHttpFilterManage.h"
using namespace std;
KBaseVirtualHost::KBaseVirtualHost()
{
	defaultRedirect = NULL;
	mimeType = NULL;
	//{{ent
#ifdef ENABLE_BLACK_LIST
	blackList = NULL;
#endif
	//}}
}
KBaseVirtualHost::~KBaseVirtualHost() {
	lock.Lock();
	std::map<char *, KBaseRedirect *, lessf>::iterator it;
	for (it = redirects.begin(); it != redirects.end(); it++) {
		xfree((*it).first);
		(*it).second->release();
	}
	redirects.clear();
	indexFiles.clear();
	errorPages.clear();
	std::list<KBaseAlias *>::iterator it2;
	for (it2 = aliass.begin(); it2 != aliass.end(); it2++) {
		delete (*it2);
	}
	aliass.clear();
	std::list<KPathRedirect *>::iterator it3;
	for(it3=pathRedirects.begin(); it3!=pathRedirects.end();it3++){
		(*it3)->release();
	}
	pathRedirects.clear();
	if(defaultRedirect){
		defaultRedirect->release();
		defaultRedirect = NULL;
	}	
	if (mimeType) {
		delete mimeType;
		mimeType = NULL;
	}
	//{{ent
#ifdef ENABLE_BLACK_LIST
	if (blackList) {
		blackList->release();
	}
#endif
	//}}
	lock.Unlock();
	clearEnv();
}
void KBaseVirtualHost::swap(KBaseVirtualHost *a)
{
	lock.Lock();
	indexFiles.swap(a->indexFiles);
	errorPages.swap(a->errorPages);
	redirects.swap(a->redirects);
	pathRedirects.swap(a->pathRedirects);
	aliass.swap(a->aliass);
	KBaseRedirect *tdefaultRedirect = defaultRedirect;
	defaultRedirect = a->defaultRedirect;
	a->defaultRedirect = tdefaultRedirect;
	if (mimeType) {
		if (a->mimeType==NULL) {
			delete mimeType;
			mimeType = NULL;
		} else {
			mimeType->swap(a->mimeType);
		}
	} else if (a->mimeType) {
		mimeType = new KMimeType;
		mimeType->swap(a->mimeType);
	}
	envs.swap(a->envs);
	lock.Unlock();
}
bool KBaseVirtualHost::getErrorPage(int code, std::string &errorPage) {
	bool result = false;
	lock.Lock();
	map<int, KBaseString>::iterator it = errorPages.find(code);
	if (it != errorPages.end()) {
		errorPage = (*it).second.s;
		result = true;
	} else {
		it = errorPages.find(0);
		if (it != errorPages.end()) {
			errorPage = (*it).second.s;
			result = true;
		}
	}
	lock.Unlock();
	return result;
}
bool KBaseVirtualHost::getIndexFile(KHttpRequest *rq,KFileName *file,KFileName **newFile,char **newPath)
{
	lock.Lock();
	for (std::list<KIndexItem>::iterator it = indexFiles.begin(); it != indexFiles.end(); it++) {
		*newFile = new KFileName;
		if ((*newFile)->setName(file->getName(), (*it).index.s.c_str(),rq->getFollowLink()) && !(*newFile)->isDirectory()) {
			(*newFile)->setIndex((*it).index.s.c_str());
			if (newPath) {
				assert(*newPath==NULL);
				*newPath = KFileName::concatDir(rq->url->path,(*it).index.s.c_str());
			}
			lock.Unlock();
			return true;
		} else {
			delete (*newFile);
		}
	}
	lock.Unlock();
	return false;
}
bool KBaseVirtualHost::delMimeType(const char *ext)
{
	bool result = false;
	lock.Lock();
	if (mimeType) {
		result = mimeType->remove(ext);
	}
	lock.Unlock();
	return result;
}
void KBaseVirtualHost::addMimeType(const char *ext,const char *type,kgl_compress_type compress,int max_age)
{
	lock.Lock();
	if (mimeType==NULL) {
		mimeType = new KMimeType;
	}
	mimeType->add(ext,type, compress,max_age);
	lock.Unlock();
}
void KBaseVirtualHost::listIndex(KVirtualHostEvent *ev)
{
	std::stringstream s;
	lock.Lock();
	for (std::list<KIndexItem>::iterator it = indexFiles.begin(); it != indexFiles.end(); it++) {
		s << "<index id='" << (*it).id << "' index='" << (*it).index.s << "' inherit='" << ((*it).index.inherited?1:0) << "'/>\n";
	}
	lock.Unlock();
	ev->add("indexs",s.str().c_str(),false);
}
void KBaseVirtualHost::getIndexHtml(std::string url, std::stringstream &s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>" << klang["id"] << "</td><td>"
			<< klang["index"] << "</td></tr>";
	for (std::list<KIndexItem>::iterator it = indexFiles.begin(); it != indexFiles.end(); it++) {
		s << "<tr><td>";
		if ((*it).index.inherited) {
			s << klang["inherited"];
		} else {
			s
					<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=indexdelete&index=";
			s << (*it).index.s << "&" << url << "';}\">" << LANG_DELETE << "</a>]";
		}
		s << "</td>";
		s << "<td>" << (*it).id << "</td>";
		s << "<td>" << (*it).index.s << "</td>";
		s << "</tr>";
	}
	s << "</table>";
	s << "<form action='/vhbase?action=indexadd&" << url << "' method='post'>";
	s << klang["id"] << ":<input name='index_id' value='100' size=4>";
	s << klang["index"] << ":<input name='index'><input type='submit' value='"
			<< LANG_ADD << "'>";
	s << "</form>";
}
void KBaseVirtualHost::getErrorPageHtml(std::string url, std::stringstream &s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< klang["error_code"];
	s << "</td><td>" << klang["error_url"] << "</td></tr>\n";
	std::map<int, KBaseString>::iterator it;
	for (it = errorPages.begin(); it != errorPages.end(); it++) {
		s << "<tr><td>";
		if ((*it).second.inherited) {
			s << klang["inherited"];
		} else {
			s
					<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=errorpagedelete&code=";
			s << (*it).first << "&" << url << "';}\">" << LANG_DELETE
					<< "</a>]";
		}
		s << "</td>";
		s << "<td>" << (*it).first << "</td>";
		s << "<td>" << (*it).second.s << "</td>";
		s << "</tr>\n";
	}
	s << "</table>\n";
	s << "<form action='/vhbase?action=errorpageadd&" << url
			<< "' method='post'>";
	s << klang["error_code"] << ":<input name='code'>";
	s << klang["error_url"] << ":<input name='url'>";
	s << "<input type='submit' value='" << LANG_ADD << "'>";
	s << "</form>";

}
void KBaseVirtualHost::getMimeTypeHtml(std::string url, std::stringstream &s)
{
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>" << klang["file_ext"] << "</td><td>";
	s << klang["mime_type"] << "</td><td>compress</td><td>" << klang["max_age"] << "</td></tr>";
	if (mimeType) {
		if(mimeType->defaultMimeType){
			mimeType->defaultMimeType->buildHtml("*",url,s);
		}
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		for(it=mimeType->mimetypes.begin();it!=mimeType->mimetypes.end();it++){
			(*it).second->buildHtml((*it).first,url,s);
		}
	}
	s << "<form action='/vhbase?action=mimetypeadd&" << url << "' method='post'>";
	s << "<table border='0'>";
	s << "<tr><td>" << klang["file_ext"] << "</td><td><input name='ext' value='' size=8></td></tr>";
	s << "<tr><td>" << klang["mime_type"] << "</td><td><input name='type'></td></tr>";
	s << "<tr><td>" << klang["max_age"] << "</td><td><input name='max_age' value='0' size=6></td></tr>";
	s << "<tr><td>compress</td><td><select name='compress'><option value=0>unknow</option><option value=1>yes</option><option value=2>never</option></select></td>";
	s << "<tr><td colspan=2><input type='submit' value='" << LANG_ADD << "'></td></tr></table>";
	s << "</form>";

}
void KBaseVirtualHost::getAliasHtml(std::string url, std::stringstream &s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< klang["id"] << "</td><td>" << klang["url_path"] << "</td>";
	s << "<td>" << klang["phy_path"] << "</td>";
	s << "<td>" << klang["internal"] << "</td>";
	s << "<td>" << LANG_HIT_COUNT << "</td></tr>\n";
	std::list<KBaseAlias *>::iterator it;
	int i = 1;
	for (it = aliass.begin(); it != aliass.end(); it++) {
		s << "<tr><td>";
		if ((*it)->inherited) {
			s << klang["inherited"];
		} else {
			s
					<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=aliasdelete&path=";
			s << (*it)->getPath() << "&" << url << "';}\">" << LANG_DELETE
					<< "</a>]";
		}
		s << "</td>";
		s << "<td>";
		if (!(*it)->inherited) {
			s << i;
			i++;
		} else {
			s << "&nbsp;";
		}
		s << "</td>";
		s << "<td>" << (*it)->getPath() << "</td>";
		s << "<td>" << (*it)->getTo() << "</td>";
		s << "<td>" << ((*it)->internal?klang["internal"]:"&nbsp;") << "</td>";
		s << "<td>" << (*it)->hit_count << "</td>";
		s << "</tr>\n";
	}
	s << "</table>\n";
	s << "<form action='/vhbase?action=aliasadd&" << url << "' method='post'>";
	s << "<table border='0'>";
	s << "<tr><td>" << klang["id"] << "</td><td><input name='index' value='" << klang["last"]
			<< "' size=4></td></tr>";
	s << "<tr><td>" << klang["url_path"] << "</td><td><input name='path'></td></tr>";
	s << "<tr><td>" << klang["phy_path"] << "</td><td><input name='to'></td></tr>";
	s << "<tr><td><input type='checkbox' name='internal' value='1'>" << klang["internal"] << "</td>";
	s << "<td><input type='submit' value='" << LANG_ADD << "'></td></tr></table>";
	s << "</form>";

}
void KBaseVirtualHost::getRedirectItemHtml(std::string url,std::string value,bool file_ext,KBaseRedirect *brd,std::stringstream &s)
{
		s << "<tr><td>";
		if (brd->inherited) {
			s << klang["inherited"];
		} else {
			s
					<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=redirectdelete&type=";
			s << (file_ext?"file_ext":"path");
			s << "&value=";
			s << url_encode(url_encode(value.c_str()).c_str());
			s << "&" << url << "';}\">" << LANG_DELETE	<< "</a>]";
		}
		s << "</td>";
		s << "<td>" << (file_ext?klang["file_ext"]:klang["path"]) << "</td>";
		s << "<td>" << value << "</td>";
		s << "<td>";
		if (brd->rd) {
			const char *rd_type = brd->rd->getType();
			if (strcmp(rd_type, "mserver") == 0) {
				rd_type = "server";
			}
			s << klang[rd_type] << ":" << brd->rd->name ;
		} else {
			s << klang["default"];
		}
		s << "</td>";
		s << "<td>" << brd->allowMethod.getMethod() << "</td>";
		s << "<td>" << kgl_get_confirm_file_tips(brd->confirmFile)	<< "</td>";
		//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
		s << "<td>";
		brd->buildParams(s);
		s << "</td>";
#endif
		//}}
		s << "</tr>";
}
void KBaseVirtualHost::getRedirectHtml(std::string url, std::stringstream &s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< klang["map_type"];
	s << "</td><td>" << LANG_VALUE << "</td><td>" << klang["extend"]
			<< "</td><td>" << klang["allow_method"] << "</td><td>"
			<< klang["confirm_file"] << "</td>";
	//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
			s << "<td>" << klang["params"] << "</td>";
#endif
	//}}
	s << "</tr>";
	std::list<KPathRedirect *>::iterator it2;
	for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		getRedirectItemHtml(url,(*it2)->path,false,(*it2),s);
	}

	std::map<char *, KBaseRedirect *, lessf>::iterator it;
	for (it = redirects.begin(); it != redirects.end(); it++) {
		getRedirectItemHtml(url,(*it).first,true,(*it).second,s);
	}
	if (defaultRedirect) {
		getRedirectItemHtml(url,"*",true,defaultRedirect,s);
	}
	s << "</table>";
	s << "<form action='/vhbase?action=redirectadd&" << url
			<< "' method='post'>";
	s << "<table border=0><tr><td>";
	s << "<select name='type'>";
	s << "<option value='file_ext'>" << klang["file_ext"] << "</option>";
	s << "<option value='path'>" << klang["path"] << "</option>";
	s << "</select>";
	s << "<input name='value'>";
	s << "</td></tr>";
	s << "<tr><td>";
	vector<std::string> targets = conf.gam->getAllTarget();
	s << klang["extend"] << ":<select name='extend'>\n";
	for (size_t i = 0; i < targets.size(); i++) {
		s << "<option value='" << targets[i] << "'>";
		char *buf = strdup(targets[i].c_str());
		char *p = strchr(buf, ':');
		if (p) {
			*p = 0;
			p++;
		}
		const char *type = klang[buf];
		if (type && *type) {
			s << type;
		} else {
			s << buf;
		}
		if (p && *p) {
			s << ":" << p;
		}
		xfree(buf);
		s << "</option>\n";
	}
	s << "<option value='default'>" << klang["default"] << "</option>";
	s << "</select>";
	s << "</td></tr>";
	s << "<tr><td>" << klang["confirm_file"];
	s << "<select name='confirm_file'>";
	for (int i = 0; i < 3; i++) {
		s << "<option value='" << i << "'>" << kgl_get_confirm_file_tips(i);
	}
	s << "</select>";
	s << "</td></tr>";
	s << "<tr><td>" << klang["allow_method"] << ":<input name='allow_method' value='*'></td></tr>";
	//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
	s << "<tr><td>" << klang["params"] << ":<input name='params' size=40></td></tr>";
#endif
	//}}
	s << "<tr><td>";
	s << "<input type='submit' value='" << LANG_ADD << "'>";
	s << "</td></tr></table>";
	s << "</form>";
}
bool KBaseVirtualHost::addIndexFile(std::string index,int id) {
	lock.Lock();
	std::list<KIndexItem>::iterator it,it_insert;
	bool it_insert_finded = false;
	for (it = indexFiles.begin();it!=indexFiles.end(); it++) {
		if ((*it).index.s == index) {
			//exsit
			indexFiles.erase(it);
			break;
		}
	}
	for (it = indexFiles.begin();it!=indexFiles.end(); it++) {
		if (!it_insert_finded && (*it).id > id) {
			it_insert = it;
			it_insert_finded = true;
			break;
		}
	}
	KIndexItem item;
	item.id = id;
	item.index.s = index;
	if (it_insert_finded) {
		indexFiles.insert(it_insert,item);
	} else {
		indexFiles.push_back(item);
	}
	lock.Unlock();

	return true;
}
bool KBaseVirtualHost::delIndexFile(std::string index)
{
	lock.Lock();
	std::list<KIndexItem>::iterator it;
	for (it = indexFiles.begin();it!=indexFiles.end();it++) {
		if ((*it).index.s == index) {
			//exsit
			indexFiles.erase(it);
			lock.Unlock();
			return true;
		}
	}
	lock.Unlock();
	return false;

}
/*
bool KBaseVirtualHost::delIndexFile(size_t index) {
	bool result = false;
	lock.Lock();
	if (index >= 0 && index < indexFiles.size()) {
		result = true;
		indexFiles.erase(indexFiles.begin() + index);
	}
	lock.Unlock();
	return result;
}
*/
bool KBaseVirtualHost::addRedirect(bool file_ext,std::string value,KRedirect *rd,std::string allowMethod, uint8_t confirmFile,std::string params)
{
	lock.Lock();
	if (file_ext) {
		if (value=="*") {
			//Ä¬ÈÏÀ©Õ¹
				KBaseRedirect *brd = new KBaseRedirect(rd, confirmFile);
				brd->allowMethod.setMethod(allowMethod.c_str());
				if (defaultRedirect) {
					defaultRedirect->release();
				}
				defaultRedirect = brd;
		} else {
			std::map<char *, KBaseRedirect *, lessf>::iterator it;
			it = redirects.find((char *) value.c_str());
			if (it != redirects.end()) {
				xfree((*it).first);
				(*it).second->release();
				redirects.erase(it);
			}
			KBaseRedirect *nrd = new KBaseRedirect(rd, confirmFile);
			nrd->allowMethod.setMethod(allowMethod.c_str());
		//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
			nrd->parseParams(params);
#endif
		//}}
			redirects.insert(pair<char *, KBaseRedirect *> (xstrdup(value.c_str()), nrd));
		}		
	} else {
		KPathRedirect *pr = new KPathRedirect(value.c_str(), rd);
		pr->allowMethod.setMethod(allowMethod.c_str());
		pr->confirmFile = confirmFile;
		//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
		pr->parseParams(params);
#endif
		//}}
		list<KPathRedirect *>::iterator it2;
		bool finded = false;
		for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
			if (filecmp((*it2)->path, value.c_str()) == 0) {
				finded = true;
				pathRedirects.insert(it2,pr);
				(*it2)->release();
				pathRedirects.erase(it2);
				break;
			}
		}
		if (!finded) {
			pathRedirects.push_back(pr);
		}
	}
	lock.Unlock();
	return true;
}
bool KBaseVirtualHost::addRedirect(bool file_ext, std::string value, std::string extend, std::string allowMethod, uint8_t confirmFile,std::string params) {

	KRedirect *rd = NULL;
	if(extend!="default"){
		rd = conf.gam->refsRedirect(extend);
		if (rd == NULL) {
			return false;
		}
	}
	return addRedirect(file_ext,value,rd,allowMethod,confirmFile,params);
}
bool KBaseVirtualHost::delRedirect(bool file_ext, std::string value) {
	bool result = false;
	lock.Lock();
	if (file_ext) {
		if (value=="*") {
			if (defaultRedirect) {
				defaultRedirect->release();
				defaultRedirect = NULL;
			}
		} else {
			std::map<char *, KBaseRedirect *, lessf>::iterator it;
			it = redirects.find((char *) value.c_str());
			if (it != redirects.end()) {
				xfree((*it).first);
				//delete (*it).second;
				(*it).second->release();
				redirects.erase(it);
				result = true;
			}
		}
	} else {
		list<KPathRedirect *>::iterator it2;
		for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
			if (filecmp((*it2)->path, value.c_str()) == 0) {
				if ((*it2)->rd) {
					(*it2)->rd->release();
				}
				pathRedirects.erase(it2);
				result = true;
				break;
			}
		}
	}
	lock.Unlock();
	return result;
}
bool KBaseVirtualHost::addErrorPage(int code, std::string url) {
	lock.Lock();
	errorPages[code] = url;
	errorPages[code].inherited = false;
	lock.Unlock();
	return true;
}
bool KBaseVirtualHost::delErrorPage(int code) {
	bool result = false;
	lock.Lock();
	map<int, KBaseString>::iterator it = errorPages.find(code);
	if (it != errorPages.end()) {
		errorPages.erase(it);
		result = true;
	}
	lock.Unlock();
	return result;
}
void KBaseVirtualHost::buildBaseXML(std::stringstream &s) {
	/*
	s << " index='";
	for (std::list<KIndexItem>::iterator iit = indexFiles.begin(); iit != indexFiles.end(); iit++) {
		if ((*iit).index.inherited) {
			continue;
		}
		if (i > 0) {
			s << ",";
		}
		s << indexFiles[i].s;
	}
	s << "'";
	*/
	std::map<int, KBaseString>::iterator it;
	for (it = errorPages.begin(); it != errorPages.end(); it++) {
		if ((*it).second.inherited) {
			continue;
		}
		s << " error_" << (*it).first << "='" << (*it).second.s << "'";
	}
	s << ">\n";
	for (std::list<KIndexItem>::iterator iit = indexFiles.begin(); iit != indexFiles.end(); iit++) {
		if ((*iit).index.inherited) {
			continue;
		}
		s << "<index id='" << (*iit).id << "' file='" << (*iit).index.s << "'/>\n";
	}
	if(envs.size()>0){
		s << "<env ";
		buildEnv(s);
		s << "/>\n";
	}
	std::list<KBaseAlias *>::iterator it4;
	for (it4 = aliass.begin(); it4 != aliass.end(); it4++) {
		if ((*it4)->inherited) {
			continue;
		}
		s << "<alias path='" << KXml::param((*it4)->getPath());
		s << "' to='" << KXml::param((*it4)->getTo()) << "'";
		if((*it4)->internal){
			s << " internal='1'";
		}
		s << "/>\n";
	}
	std::list<KPathRedirect *>::iterator it3;
	for (it3 = pathRedirects.begin(); it3 != pathRedirects.end(); it3++) {
		if ((*it3)->inherited) {
			continue;
		}
		s << "<map path='" << (*it3)->path << "'";
		(*it3)->buildXML(s);
	}
	std::map<char *, KBaseRedirect *, lessf>::iterator it2;
	for (it2 = redirects.begin(); it2 != redirects.end(); it2++) {
		if ((*it2).second->inherited) {
			continue;
		}
		s << "<map file_ext='" << (*it2).first << "'";
		(*it2).second->buildXML(s);
	}
	if (defaultRedirect) {
		s << "<map file_ext='*' ";
		defaultRedirect->buildXML(s);
	}
	if (mimeType) {
		mimeType->buildXml(s);
	}
}
bool KBaseVirtualHost::addAlias(std::string path, std::string to,const char *doc_root,bool internal, int id,
		std::string &errMsg) {
	if (path.size() <= 0 || to.size() <= 0) {
		errMsg = "path or to cann't be zero";
		return false;
	}
	if (path[0] != '/') {
		errMsg = "path not start with `/` char";
		return false;
	}
	KBaseAlias *alias = new KBaseAlias(path.c_str(), to.c_str(),doc_root);
	alias->internal = internal;
	lock.Lock();
	list<KBaseAlias *>::iterator it, it2;
	bool fined = false;
	int i = 1;
	for (it = aliass.begin(); it != aliass.end(); it++, i++) {
		if (alias->equalPath((*it))) {
			lock.Unlock();
			delete alias;
			errMsg = "path is exsit";
			return false;
		}
		if (i == id) {
			fined = true;
			it2 = it;
		}
	}

	if (fined) {
		aliass.insert(it2, alias);
	} else {
		aliass.push_back(alias);
	}
	lock.Unlock();
	return true;
}
bool KBaseVirtualHost::delAlias(const char *path) {
	lock.Lock();
	list<KBaseAlias *>::iterator it, it2;
	int i = 1;
	for (it = aliass.begin(); it != aliass.end(); it++, i++) {
		if ((*it)->inherited) {
			continue;
		}
		if (filecmp(path, (*it)->getPath()) == 0) {
			aliass.erase(it);
			lock.Unlock();
			return true;
		}
	}
	lock.Unlock();
	return false;
}
void KBaseVirtualHost::copyTo(KVirtualHost *vh, bool copyInherit, int inherited) {
	std::map<char *, KBaseRedirect *, lessf>::iterator it;
	for (it = redirects.begin(); it != redirects.end(); it++) {
		if ((*it).second->inherited && !copyInherit) {
			continue;
		}
		if (vh->redirects.find((*it).first) == vh->redirects.end()) {
			if((*it).second->rd){
				(*it).second->rd->addRef();
			}
			KBaseRedirect *br = new KBaseRedirect((*it).second->rd,
					(*it).second->confirmFile);
			if (inherited == 2) {
				br->inherited = (*it).second->inherited;
			} else {
				br->inherited = (inherited == 1);
			}
			//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
			br->params = (*it).second->params;
#endif
			//}}
			br->allowMethod.setMethod(
					(*it).second->allowMethod.getMethod().c_str());
			vh->redirects.insert(pair<char *, KBaseRedirect *> (
					xstrdup((*it).first), br));
		}
	}
	list<KPathRedirect *>::iterator it2;
	for (it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		if ((*it2)->inherited && !copyInherit) {
			continue;
		}
		list<KPathRedirect *>::iterator it3;
		bool finded = false;
		for (it3 = vh->pathRedirects.begin(); it3 != vh->pathRedirects.end(); it3++) {
			if (filecmp((*it2)->path, (*it3)->path) == 0) {
				finded = true;
				break;
			}
		}
		if (finded) {
			continue;
		}
		if ((*it2)->rd) {
			(*it2)->rd->addRef();
		}
		KPathRedirect *prd = new KPathRedirect((*it2)->path, (*it2)->rd);
		prd->confirmFile = (*it2)->confirmFile;
		//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
		prd->params = (*it2)->params;
#endif
		//}}
		prd->allowMethod.setMethod((*it2)->allowMethod.getMethod().c_str());

		if (inherited == 2) {
			prd->inherited = (*it2)->inherited;
		} else {
			prd->inherited = (inherited == 1);
		}
		vh->pathRedirects.push_back(prd);
	}
	for (std::list<KIndexItem>::iterator iit = indexFiles.begin(); iit != indexFiles.end(); iit++) {
		if ((*iit).index.inherited && !copyInherit) {
			continue;
		}
		bool finded = false;
		for (std::list<KIndexItem>::iterator iit2 = vh->indexFiles.begin(); iit2 != vh->indexFiles.end(); iit2++) {
			if (filecmp((*iit).index.s.c_str(), (*iit2).index.s.c_str())
					== 0) {
				finded = true;
				break;
			}
		}
		if (finded) {
			continue;
		}
		KIndexItem item;
		if (inherited == 2) {
			item.index.inherited = (*iit).index.inherited;
		} else {
			item.index.inherited = (inherited == 1);
		}
		item.id = (*iit).id;
		item.index.s = (*iit).index.s;
		std::list<KIndexItem>::iterator itp;
		bool it_insert_finded = false;
		for (itp = vh->indexFiles.begin();itp!=vh->indexFiles.end(); itp++) {
			if (!it_insert_finded && (*itp).id > item.id) {
				it_insert_finded = true;
				break;
			}
		}
		if (it_insert_finded) {
			vh->indexFiles.insert(itp,item);
		} else {
			vh->indexFiles.push_back(item);
		}
	}
	map<int, KBaseString>::iterator it4;
	for (it4 = errorPages.begin(); it4 != errorPages.end(); it4++) {
		if ((*it4).second.inherited && !copyInherit) {
			continue;
		}
		if (vh->errorPages.find((*it4).first) == vh->errorPages.end()) {
			KBaseString bs;
			if (inherited == 2) {
				bs.inherited = (*it4).second.inherited;
			} else {
				bs.inherited = (inherited == 1);
			}
			bs.s = (*it4).second.s;
			vh->errorPages.insert(
					pair<int, KBaseString> ((*it4).first, bs));
		}
	}

	std::list<KBaseAlias *>::iterator it5;
	for (it5 = aliass.begin(); it5 != aliass.end(); it5++) {
		std::list<KBaseAlias *>::iterator it6;
		bool finded = false;
		for (it6 = vh->aliass.begin(); it6 != vh->aliass.end(); it6++) {
			if ((*it5)->equalPath((*it6))) {
				finded = true;
				break;
			}
		}
		if (finded) {
			continue;
		}
		KBaseAlias *alias = new KBaseAlias;
		(*it5)->cloneTo(alias,vh->doc_root.c_str());
		if (inherited == 2) {
			alias->inherited = (*it5)->inherited;
		} else {
			alias->inherited = (inherited == 1);
		}
		vh->aliass.push_back(alias);
	}
	if(defaultRedirect){
		if(vh->defaultRedirect==NULL){
			vh->defaultRedirect = new KBaseRedirect(defaultRedirect->rd,defaultRedirect->confirmFile);
			if (inherited == 2) {
				vh->defaultRedirect->inherited = defaultRedirect->inherited;
			} else {
				vh->defaultRedirect->inherited = (inherited == 1);
			}
		}
	}
}
void KBaseVirtualHost::cloneTo(KVirtualHost *vh, bool copyInherit,
		int changeInherit) {
	vh->lock.Lock();
	copyTo(vh, copyInherit, changeInherit);
	vh->lock.Unlock();
}
void KBaseVirtualHost::inheriTo(KVirtualHost *vh, bool clearFlag) {
	if (!vh->inherit) {
		return;
	}
	vh->lock.Lock();
	if (clearFlag) {
		vh->internalChangeInherit(true);
	}
	copyTo(vh, true, true);
	vh->lock.Unlock();
}
bool KBaseVirtualHost::alias(bool internal,const char *path,KFileName *file,bool &exsit,int flag) {
	int len = strlen(path);
	lock.Lock();
	std::list<KBaseAlias *>::iterator it;
	for (it = aliass.begin(); it != aliass.end(); it++) {
		if ((*it)->internal) {
			if (!internal) {
				continue;
			}
		}
		if((*it)->match(path, len)){
			exsit = file->setName((*it)->to,path + (*it)->path_len,flag);
			lock.Unlock();
			return true;
		}
	}
	lock.Unlock();
	return false;
}
char *KBaseVirtualHost::alias(bool internal,const char *path) {
	int len = strlen(path);
	lock.Lock();
	std::list<KBaseAlias *>::iterator it;
	for (it = aliass.begin(); it != aliass.end(); it++) {
		if ((*it)->internal) {
			if (!internal) {
				continue;
			}
		}
		if ((*it)->match(path, len)) {
			char *new_path = (*it)->matched(path,len);
			lock.Unlock();
			return new_path;
		}
	}
	lock.Unlock();
	return NULL;
}
void KBaseVirtualHost::changeInherit(bool remove) {
	lock.Lock();
	internalChangeInherit(remove);
	lock.Unlock();
}
void KBaseVirtualHost::internalChangeInherit(bool remove) {

	std::map<char *, KBaseRedirect *, lessf>::iterator it, itnext;
	for (it = redirects.begin(); it != redirects.end();) {
		if ((*it).second->inherited) {
			if (remove) {
				itnext = it;
				itnext++;
				//delete (*it).second;
				(*it).second->release();
				redirects.erase(it);
				it = itnext;
			} else {
				(*it).second->inherited = false;
				it++;
			}
		} else {
			it++;
		}
	}
	list<KPathRedirect *>::iterator it2;
	for (it2 = pathRedirects.begin(); it2 != pathRedirects.end();) {
		if ((*it2)->inherited) {
			if (remove) {
				//delete (*it2);
				(*it2)->release();
				it2 = pathRedirects.erase(it2);
			} else {
				(*it2)->inherited = false;
				it2++;
			}
		} else {
			it2++;
		}
	}
	map<int, KBaseString>::iterator it3, it3next;
	for (it3 = errorPages.begin(); it3 != errorPages.end();) {
		if ((*it3).second.inherited) {
			if (remove) {
				it3next = it3;
				it3next++;
				errorPages.erase(it3);
				it3 = it3next;
			} else {
				(*it3).second.inherited = false;
				it3++;
			}
		} else {
			it3++;
		}
	}
	list<KIndexItem>::iterator it4;
	for (it4 = indexFiles.begin(); it4 != indexFiles.end();) {
		if ((*it4).index.inherited) {
			if (remove) {
				it4 = indexFiles.erase(it4);
			} else {
				(*it4).index.inherited = false;
				it4++;
			}
		} else {
			it4++;
		}
	}
	list<KBaseAlias *>::iterator it5;
	for (it5 = aliass.begin(); it5 != aliass.end();) {
		if (!(*it5)->inherited) {
			it5++;
			continue;
		}
		if (remove) {
			delete (*it5);
			it5 = aliass.erase(it5);
		} else {
			(*it5)->inherited = false;
			it5++;
		}
	}
}
void KBaseVirtualHost::getParsedFileExt(KVirtualHostEvent *ctx)
{
	lock.Lock();
	std::map<char *, KBaseRedirect *, lessf>::iterator it2;
	for(it2=redirects.begin();it2!=redirects.end();it2++){
		ctx->add("file_ext",(*it2).first);
	}
	lock.Unlock();
}
bool KBaseVirtualHost::addEnvValue(const char *name,const char *value)
{
	if(name==NULL || value==NULL){
		return false;
	}
	map<char *,char *,lessp_icase>::iterator it;
	bool result = false;
	//KMutex *lock = getEnvLock();
	lock.Lock();
	it = envs.find((char *)name);
	if(it==envs.end()){
		envs.insert(pair<char *,char *>(xstrdup(name),xstrdup(value)));
		result = true;
	}
	lock.Unlock();
	return result;
}
bool KBaseVirtualHost::getEnvValue(const char *name,std::string &value) {
	if (name == NULL) {
		return false;
	}
	//KMutex *lock = getEnvLock();

	lock.Lock();
	std::map<char *, char *, lessp_icase>::iterator it;
	it = envs.find((char *) name);
	if (it != envs.end()) {
		value = (*it).second;
		lock.Unlock();
		return true;
	}
	lock.Unlock();
	return false;
}
void KBaseVirtualHost::clearEnv()
{
//	KMutex *lock = getEnvLock();
	lock.Lock();
	std::map<char *, char *, lessp_icase>::iterator it3;
	for (it3 = envs.begin(); it3 != envs.end(); it3++) {
		xfree((*it3).second);
		xfree((*it3).first);
	}
	envs.clear();
	lock.Unlock();
}
void KBaseVirtualHost::getErrorEnv(const char *split,KStringBuf &s)
{
	std::map<int, KBaseString>::iterator it2;
	lock.Lock();
	for (it2=errorPages.begin();it2!=errorPages.end();it2++) {
		if (s.getSize()>0) {
			s << split;
		}
		s << (*it2).first << "=" << (*it2).second.s;
	}
	lock.Unlock();
}
void KBaseVirtualHost::getIndexFileEnv(const char *split,KStringBuf &s)
{
	std::list<KIndexItem>::iterator it;
	lock.Lock();
	for (it=indexFiles.begin();it!=indexFiles.end();it++) {
		if (s.getSize()>0) {
			s << split;
		}
		s << (*it).index.s;
	}
	lock.Unlock();
}
