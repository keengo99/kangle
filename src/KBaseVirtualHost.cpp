#include <vector>
#include <sstream>
#include <map>
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
#include "KDefer.h"
using namespace std;
void on_vh_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	assert(data);
	KBaseVirtualHost* bvh = (KBaseVirtualHost*)data;
	bvh->on_config_event(tree, ev);
}
KBaseVirtualHost::KBaseVirtualHost() {
	mimeType = NULL;
#ifdef ENABLE_BLACK_LIST
	blackList = NULL;
#endif
}
KFetchObject* KBaseVirtualHost::find_file_redirect(KHttpRequest* rq, KFileName* file, const char* file_ext, bool fileExsit, bool& result) {
	decltype (redirects.find((char *)"")) it;
	auto locker = get_locker();
	if (!file_ext) {
		it = redirects.find((char *)"*");
	} else {
		it = redirects.find((char *)file_ext);
		if (it == redirects.end()) {
			it = redirects.find((char *)"*");
		}
	}
	if (it != redirects.end() && (*it).second->allowMethod.matchMethod(rq->sink->data.meth) && (*it).second->MatchConfirmFile(fileExsit)) {
		result = true;
		if ((*it).second->rd) {
			auto fo = (*it).second->rd->makeFetchObject(rq, file);
			(*it).second->addRef();
			fo->bind_base_redirect((*it).second);
			return fo;
		}
	}
	return nullptr;
}
KAlias KBaseVirtualHost::find_alias(bool internal, const char* path, size_t path_len) {
	auto locker = get_locker();
	for (auto it = aliass.begin(); it != aliass.end(); it++) {
		if ((*it)->internal) {
			if (!internal) {
				continue;
			}
		}
		if ((*it)->match(path, path_len)) {
			return KAlias((*it)->add_ref());
		}
	}
	return KAlias(nullptr);
}
KBaseRedirect* KBaseVirtualHost::refs_path_redirect(const char* path, int path_len) {
	auto locker = get_locker();
	for (auto it = pathRedirects.begin(); it != pathRedirects.end(); ++it) {
		if ((*it)->match(path, path_len)) {
			(*it)->addRef();
			return (*it);
		}
	}
	return nullptr;
}
KFetchObject* KBaseVirtualHost::find_path_redirect(KHttpRequest* rq, KFileName* file, const char* path, size_t path_len, bool fileExsit, bool& result) {
	auto locker = get_locker();
	for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end(); ++it2) {
		if ((*it2)->match(path, path_len) && (*it2)->allowMethod.matchMethod(rq->sink->data.meth)) {
			if ((*it2)->MatchConfirmFile(fileExsit)) {
				result = true;
				if ((*it2)->rd) {
					auto fo = (*it2)->rd->makeFetchObject(rq, file);
					(*it2)->addRef();
					fo->bind_base_redirect((*it2));
					return fo;
				}
				return nullptr;
			}
		}
	}
	return nullptr;
}
void KBaseVirtualHost::clear() {
	auto locker = get_locker();
	for (auto it = redirects.begin(); it != redirects.end(); ++it) {
		xfree((*it).first);
		(*it).second->release();
	}
	redirects.clear();
	indexFiles.clear();
	errorPages.clear();
	for (auto it2 = aliass.begin(); it2 != aliass.end(); it2++) {
		(*it2)->release();
	}
	aliass.clear();
	for (auto it3 = pathRedirects.begin(); it3 != pathRedirects.end(); it3++) {
		(*it3)->release();
	}
	pathRedirects.clear();
#if 0
	if (defaultRedirect) {
		defaultRedirect->release();
		defaultRedirect = NULL;
	}
#endif
	if (mimeType) {
		delete mimeType;
		mimeType = NULL;
	}
	for (auto it3 = envs.begin(); it3 != envs.end(); it3++) {
		xfree((*it3).second);
		xfree((*it3).first);
	}
	envs.clear();
}
KBaseVirtualHost::~KBaseVirtualHost() {
	clear();
#ifdef ENABLE_BLACK_LIST
	if (blackList) {
		blackList->release();
		blackList = nullptr;
	}
#endif
}
void KBaseVirtualHost::swap(KBaseVirtualHost* a) {
	lock.Lock();
	indexFiles.swap(a->indexFiles);
	errorPages.swap(a->errorPages);
	redirects.swap(a->redirects);
	pathRedirects.swap(a->pathRedirects);
	aliass.swap(a->aliass);
#if 0
	KBaseRedirect* tdefaultRedirect = defaultRedirect;
	defaultRedirect = a->defaultRedirect;
	a->defaultRedirect = tdefaultRedirect;
#endif
	if (mimeType) {
		if (a->mimeType == NULL) {
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
bool KBaseVirtualHost::getErrorPage(int code, std::string& errorPage) {
	{
		auto locker = get_locker();
		auto it = errorPages.find(code);
		if (it != errorPages.end()) {
			errorPage = (*it).second.s;
			return true;
		} else {
			it = errorPages.find(0);
			if (it != errorPages.end()) {
				errorPage = (*it).second.s;
				return true;
			}
		}
	}
	if (!is_global()) {
		auto vh = static_cast<KVirtualHost*>(this);
		if (vh->inherit) {
			return conf.gvm->vhs.getErrorPage(code, errorPage);
		}
	}
	return false;
}
bool KBaseVirtualHost::getIndexFile(KHttpRequest* rq, KFileName* file, KFileName** newFile, char** newPath) {
	{
		auto locker = get_locker();
		for (auto it = indexFiles.begin(); it != indexFiles.end(); ++it) {
			*newFile = new KFileName;
			if ((*newFile)->setName(file->getName(), (*it).index.s.c_str(), rq->getFollowLink()) && !(*newFile)->isDirectory()) {
				(*newFile)->setIndex((*it).index.s.c_str());
				if (newPath) {
					assert(*newPath == NULL);
					*newPath = KFileName::concatDir(rq->sink->data.url->path, (*it).index.s.c_str());
				}
				return true;
			} else {
				delete (*newFile);
			}
		}
	}
	if (!is_global()) {
		auto vh = static_cast<KVirtualHost*>(this);
		if (vh->inherit) {
			return conf.gvm->vhs.getIndexFile(rq, file, newFile, newPath);
		}
	}
	return false;
}
bool KBaseVirtualHost::delMimeType(const char* ext) {
	bool result = false;
	auto locker = get_locker();
	if (mimeType) {
		result = mimeType->remove(ext);
	}
	return result;
}
void KBaseVirtualHost::addMimeType(const char* ext, const char* type, kgl_compress_type compress, int max_age) {
	lock.Lock();
	if (mimeType == NULL) {
		mimeType = new KMimeType;
	}
	mimeType->add(ext, type, compress, max_age);
	lock.Unlock();
}
void KBaseVirtualHost::listIndex(KVirtualHostEvent* ev) {
	std::stringstream s;
	lock.Lock();
	for (auto it = indexFiles.begin(); it != indexFiles.end(); it++) {
		s << "<index id='" << (*it).id << "' index='" << (*it).index.s << "' inherit='" << ((*it).index.inherited ? 1 : 0) << "'/>\n";
	}
	lock.Unlock();
	ev->add("indexs", s.str().c_str(), false);
}
void KBaseVirtualHost::getIndexHtml(const std::string& url, std::stringstream& s) {
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
void KBaseVirtualHost::getErrorPageHtml(const std::string& url, std::stringstream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
		<< klang["error_code"];
	s << "</td><td>" << klang["error_url"] << "</td></tr>\n";
	for (auto it = errorPages.begin(); it != errorPages.end(); it++) {
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
void KBaseVirtualHost::getMimeTypeHtml(const std::string& url, std::stringstream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>" << klang["file_ext"] << "</td><td>";
	s << klang["mime_type"] << "</td><td>compress</td><td>" << klang["max_age"] << "</td></tr>";
	if (mimeType) {
		if (mimeType->defaultMimeType) {
			mimeType->defaultMimeType->buildHtml("*", url, s);
		}
		for (auto it = mimeType->mimetypes.begin(); it != mimeType->mimetypes.end(); it++) {
			(*it).second->buildHtml((*it).first, url, s);
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
void KBaseVirtualHost::getAliasHtml(const std::string& url, std::stringstream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
		<< klang["id"] << "</td><td>" << klang["url_path"] << "</td>";
	s << "<td>" << klang["phy_path"] << "</td>";
	s << "<td>" << klang["internal"] << "</td>";
	s << "<td>" << LANG_HIT_COUNT << "</td></tr>\n";
	int i = 1;
	for (auto it = aliass.begin(); it != aliass.end(); it++) {
		s << "<tr><td>";
		if ((*it)->inherited) {
			s << klang["inherited"];
		} else {
			s
				<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=aliasdelete&path=";
			s << (*it)->get_path() << "&" << url << "';}\">" << LANG_DELETE
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
		s << "<td>" << (*it)->get_path() << "</td>";
		s << "<td>" << (*it)->get_to() << "</td>";
		s << "<td>" << ((*it)->internal ? klang["internal"] : "&nbsp;") << "</td>";
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
void KBaseVirtualHost::getRedirectItemHtml(const std::string& url, const std::string& value, bool file_ext, KBaseRedirect* brd, std::stringstream& s) {
	s << "<tr><td>";
	if (brd->global) {
		s << klang["inherited"];
	} else {
		s
			<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=redirectdelete&type=";
		s << (file_ext ? "file_ext" : "path");
		s << "&value=";
		s << url_encode(url_encode(value.c_str()).c_str());
		s << "&" << url << "';}\">" << LANG_DELETE << "</a>]";
	}
	s << "</td>";
	s << "<td>" << (file_ext ? klang["file_ext"] : klang["path"]) << "</td>";
	s << "<td>" << value << "</td>";
	s << "<td>";
	if (brd->rd) {
		const char* rd_type = brd->rd->getType();
		if (strcmp(rd_type, "mserver") == 0) {
			rd_type = "server";
		}
		s << klang[rd_type] << ":" << brd->rd->name;
	} else {
		s << klang["default"];
	}
	s << "</td>";
	s << "<td>" << brd->allowMethod.getMethod() << "</td>";
	s << "<td>" << kgl_get_confirm_file_tips(brd->confirm_file) << "</td>";
	//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
	s << "<td>";
	brd->buildParams(s);
	s << "</td>";
#endif
	//}}
	s << "</tr>";
}
void KBaseVirtualHost::getRedirectHtml(const std::string& url, std::stringstream& s) {
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
	for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		getRedirectItemHtml(url, (*it2)->path, false, (*it2), s);
	}
	for (auto it = redirects.begin(); it != redirects.end(); it++) {
		getRedirectItemHtml(url, (*it).first, true, (*it).second, s);
	}
	//if (defaultRedirect) {
	//	getRedirectItemHtml(url, "*", true, defaultRedirect, s);
	//}
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
		char* buf = strdup(targets[i].c_str());
		char* p = strchr(buf, ':');
		if (p) {
			*p = 0;
			p++;
		}
		const char* type = klang[buf];
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
		s << "<option value='" << i << "'>" << kgl_get_confirm_file_tips(static_cast<KConfirmFile>(i));
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
bool KBaseVirtualHost::addIndexFile(const std::string& index, int id) {
	auto locker = get_locker();
	std::list<KIndexItem>::iterator it, it_insert;
	bool it_insert_finded = false;
	for (it = indexFiles.begin(); it != indexFiles.end(); it++) {
		if ((*it).index.s == index) {
			//exsit
			indexFiles.erase(it);
			break;
		}
	}
	for (it = indexFiles.begin(); it != indexFiles.end(); it++) {
		if (!it_insert_finded && (*it).id > id) {
			it_insert = it;
			it_insert_finded = true;
			break;
		}
	}
	if (it_insert_finded) {
		indexFiles.emplace(it_insert, id, index);
	} else {
		indexFiles.emplace_back(id, index);
	}
	return true;
}
bool KBaseVirtualHost::delIndexFile(const std::string& index) {
	lock.Lock();
	for (auto it = indexFiles.begin(); it != indexFiles.end(); it++) {
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
bool KBaseVirtualHost::addRedirect(bool file_ext, const std::string& value, KRedirect* rd, const std::string& allowMethod, KConfirmFile confirmFile, const std::string& params) {
	auto locker = get_locker();
	if (file_ext) {
			auto it = redirects.find((char*)value.c_str());
			if (it != redirects.end()) {
				xfree((*it).first);
				(*it).second->release();
				redirects.erase(it);
			}
			KBaseRedirect* nrd = new KBaseRedirect(rd, confirmFile);
			nrd->allowMethod.setMethod(allowMethod.c_str());
#ifdef ENABLE_UPSTREAM_PARAM
			nrd->parseParams(params);
#endif
			redirects.insert(pair<char*, KBaseRedirect*>(xstrdup(value.c_str()), nrd));
		//}
	} else {
		KPathRedirect* pr = new KPathRedirect(value.c_str(), rd);
		pr->allowMethod.setMethod(allowMethod.c_str());
		pr->confirm_file = confirmFile;
#ifdef ENABLE_UPSTREAM_PARAM
		pr->parseParams(params);
#endif
		bool finded = false;
		for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
			if (filecmp((*it2)->path, value.c_str()) == 0) {
				finded = true;
				pathRedirects.insert(it2, pr);
				(*it2)->release();
				pathRedirects.erase(it2);
				break;
			}
		}
		if (!finded) {
			pathRedirects.push_back(pr);
		}
	}
	return true;
}
bool KBaseVirtualHost::addRedirect(bool file_ext, const std::string& value, const std::string& extend, const std::string& allowMethod, KConfirmFile confirmFile, const std::string& params) {

	KRedirect* rd = NULL;
	if (extend != "default") {
		rd = conf.gam->refsRedirect(extend);
		if (rd == NULL) {
			return false;
		}
	}
	return addRedirect(file_ext, value, rd, allowMethod, confirmFile, params);
}
bool KBaseVirtualHost::addErrorPage(int code, const std::string& url) {
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
bool KBaseVirtualHost::addAlias(const std::string& path, const std::string& to, const char* doc_root, bool internal, int id,
	std::string& errMsg) {
	if (path.empty() || to.empty()) {
		errMsg = "path or to cann't be zero";
		return false;
	}
	if (path[0] != '/') {
		errMsg = "path not start with `/` char";
		return false;
	}
	KAlias alias(new KBaseAlias(path.c_str(), to.c_str(),internal));
	lock.Lock();
	list<KAlias>::iterator it, it2;
	bool fined = false;
	int i = 1;
	for (it = aliass.begin(); it != aliass.end(); it++, i++) {
		if (alias->same_path((*it).get())) {
			lock.Unlock();
			errMsg = "path is exsit";
			return false;
		}
		if (i == id) {
			fined = true;
			it2 = it;
		}
	}

	if (fined) {
		aliass.insert(it2, std::move(alias));
	} else {
		aliass.push_back(std::move(alias));
	}
	lock.Unlock();
	return true;
}
void KBaseVirtualHost::cloneTo(KVirtualHost* vh, bool copyInherit, int changeInherit) {
#if 0
	vh->lock.Lock();
	copyTo(vh, copyInherit, changeInherit);
	vh->lock.Unlock();
#endif
}
void KBaseVirtualHost::inheriTo(KVirtualHost* vh, bool clearFlag) {
	if (!vh->inherit) {
		return;
	}
#if 0
	vh->lock.Lock();
	if (clearFlag) {
		vh->internalChangeInherit(true);
	}
	copyTo(vh, true, true);
	vh->lock.Unlock();
#endif
}
void KBaseVirtualHost::changeInherit(bool remove) {
	lock.Lock();
	internalChangeInherit(remove);
	lock.Unlock();
}
void KBaseVirtualHost::internalChangeInherit(bool remove) {
#if 0
	for (auto it = redirects.begin(); it != redirects.end();) {
		if ((*it).second->global) {
			if (remove) {
				auto itnext = it;
				itnext++;
				//delete (*it).second;
				(*it).second->release();
				redirects.erase(it);
				it = itnext;
			} else {
				(*it).second->global = false;
				it++;
			}
		} else {
			it++;
		}
	}
	for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end();) {
		if ((*it2)->global) {
			if (remove) {
				//delete (*it2);
				(*it2)->release();
				it2 = pathRedirects.erase(it2);
			} else {
				(*it2)->global = false;
				it2++;
			}
		} else {
			it2++;
		}
	}
	for (auto it3 = errorPages.begin(); it3 != errorPages.end();) {
		if ((*it3).second.inherited) {
			if (remove) {
				auto it3next = it3;
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
	for (auto it4 = indexFiles.begin(); it4 != indexFiles.end();) {
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
	for (auto it5 = aliass.begin(); it5 != aliass.end();) {
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
#endif
}
void KBaseVirtualHost::getParsedFileExt(KVirtualHostEvent* ctx) {
	lock.Lock();
	for (auto it2 = redirects.begin(); it2 != redirects.end(); it2++) {
		ctx->add("file_ext", (*it2).first);
	}
	lock.Unlock();
}
bool KBaseVirtualHost::addEnvValue(const char* name, const char* value) {
	if (name == NULL || value == NULL) {
		return false;
	}
	bool result = false;
	lock.Lock();
	auto it = envs.find((char*)name);
	if (it == envs.end()) {
		envs.insert(pair<char*, char*>(xstrdup(name), xstrdup(value)));
		result = true;
	}
	lock.Unlock();
	return result;
}
bool KBaseVirtualHost::getEnvValue(const char* name, std::string& value) {
	if (name == NULL) {
		return false;
	}
	lock.Lock();
	auto it = envs.find((char*)name);
	if (it != envs.end()) {
		value = (*it).second;
		lock.Unlock();
		return true;
	}
	lock.Unlock();
	return false;
}
void KBaseVirtualHost::getErrorEnv(const char* split, KStringBuf& s) {
	lock.Lock();
	for (auto it2 = errorPages.begin(); it2 != errorPages.end(); it2++) {
		if (s.size() > 0) {
			s << split;
		}
		s << (*it2).first << "=" << (*it2).second.s;
	}
	lock.Unlock();
}
void KBaseVirtualHost::getIndexFileEnv(const char* split, KStringBuf& s) {
	lock.Lock();
	for (auto it = indexFiles.begin(); it != indexFiles.end(); it++) {
		if (s.size() > 0) {
			s << split;
		}
		s << (*it).index.s;
	}
	lock.Unlock();
}
KBaseRedirect* KBaseVirtualHost::parse_file_map(const KXmlAttribute& attr) {
	auto rd = conf.gam->refsRedirect(attr["extend"]);
	if (!rd && attr["extend"] != "default") {
		klog(KLOG_ERR, "cann't find extend [%s]\n", attr("extend"));
		return nullptr;
	}
	KBaseRedirect* nrd = new KBaseRedirect(rd, static_cast<KConfirmFile>(attr.get_int("confirm_file")));
	nrd->allowMethod.setMethod(attr("allow_method"));
#ifdef ENABLE_UPSTREAM_PARAM
	nrd->parseParams(attr["params"]);
#endif
	return nrd;

}
KPathRedirect* KBaseVirtualHost::parse_path_map(const KXmlAttribute& attr) {
	auto rd = conf.gam->refsRedirect(attr["extend"]);
	if (!rd && attr["extend"]!="default") {
		klog(KLOG_ERR, "cann't find extend [%s]\n", attr("extend"));
		return nullptr;
	}
	KPathRedirect* pr = new KPathRedirect(attr("path"), rd);
	pr->allowMethod.setMethod(attr("allow_method"));
	pr->confirm_file = static_cast<KConfirmFile>(attr.get_int("confirm_file"));
#ifdef ENABLE_UPSTREAM_PARAM
	pr->parseParams(attr["params"]);
#endif
	return pr;
}
KAlias KBaseVirtualHost::parse_alias(const KXmlAttribute& attr) {
	auto path = attr["path"];
	auto to = attr["to"];
	if (path.empty() || to.empty()) {
		klog(KLOG_ERR, "parse alias error path or to is empty\n");
		//errMsg = "path or to cann't be zero";
		return nullptr;
	}
	if (path[0] != '/') {
		klog(KLOG_ERR, "parse alias error path not start with `/` char\n");
		return nullptr;
	}
	return KAlias(new KBaseAlias(path.c_str(), to.c_str(), attr.get_int("internal") == 1));
}
bool KBaseVirtualHost::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	switch (ev->type) {
	case kconfig::EvRemove:
	{
		if (is_global()) {
			clear();
			return true;
		}
		assert(false);
		auto bvh = (KBaseVirtualHost*)tree->unbind();
		assert(bvh == this);
		static_cast<KVirtualHost*>(bvh)->destroy();
		return true;
	}
	case kconfig::EvNew:
	case kconfig::EvUpdate:
		return false;
	case kconfig::EvSubDir | kconfig::EvNew:
	case kconfig::EvSubDir | kconfig::EvUpdate:
	{
		auto xml = ev->get_xml();
		auto attr = xml->attributes();
		if (xml->is_tag(_KS("error"))) {
			this->addErrorPage(attr.get_int("code"), attr("file"));
			return true;
		}
		if (xml->is_tag(_KS("mime_type"))) {
			addMimeType(attr("ext"), attr("type"), (kgl_compress_type)attr.get_int("compress"), attr.get_int("max_age"));
			return true;
		}
		if (xml->is_tag(_KS("index"))) {
			std::multimap<int, KIndexItem> indexs;
			for (uint32_t index = 0;; ++index) {
				auto body = xml->get_body(index);
				if (!body) {
					break;
				}
				auto attr2 = body->attributes;
				auto id = attr2.get_int("id");
				indexs.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(id, attr2["file"]));
			}
			auto locker = get_locker();
			this->indexFiles.clear();
			for (auto it = indexs.begin(); it != indexs.end(); ++it) {
				this->indexFiles.push_back(std::move((*it).second));
			}
			return true;
		}
		if (xml->is_tag(_KS("map"))) {
			std::map<char*, KBaseRedirect*, lessf> file_maps;
			std::list<KPathRedirect*> path_maps;
			defer(
				for (auto it = file_maps.begin(); it != file_maps.end(); ++it) {
					(*it).second->release();
					xfree((*it).first);
				}
			for (auto it = path_maps.begin(); it != path_maps.end(); ++it) {
				(*it)->release();
			});
			for (uint32_t index = 0;; ++index) {
				auto body = xml->get_body(index);
				if (!body) {
					break;
				}
				auto attr2 = body->attributes;
				auto file_ext = attr2("file_ext", nullptr);
				if (file_ext != nullptr) {
					if (file_maps.find((char*)file_ext) == file_maps.end()) {
						auto rd = parse_file_map(attr2);
						if (rd) {
							file_maps.insert(std::pair<char*, KBaseRedirect*>(strdup(file_ext), rd));
						}
					}
				} else {
					auto rd = parse_path_map(attr2);
					if (rd) {
						path_maps.push_back(rd);
					}
				}
			}
			{
				auto locker = get_locker();
				file_maps.swap(redirects);
				path_maps.swap(pathRedirects);
			}

			return true;
		}
		if (xml->is_tag(_KS("alias"))) {
			std::list<KAlias> alias;
			for (uint32_t index = 0;; ++index) {
				auto body = xml->get_body(index);
				if (!body) {
					break;
				}
				auto as = parse_alias(body->attributes);
				if (!as) {
					continue;
				}
				alias.push_back(std::move(as));
			}
			{
				auto locker = get_locker();
				this->aliass.swap(alias);
			}
			return true;
		}
		break;
	}
	case kconfig::EvSubDir | kconfig::EvRemove:
	{
		auto xml = ev->get_xml();
		auto attr = xml->attributes();
		if (xml->is_tag(_KS("error"))) {
			this->delErrorPage(attr.get_int("code"));
			return true;
		}
		if (xml->is_tag(_KS("mime_type"))) {
			delMimeType(attr("ext"));
			return true;
		}
		if (xml->is_tag(_KS("index"))) {
			auto locker = this->get_locker();
			this->indexFiles.clear();
			return true;
		}
		if (xml->is_tag(_KS("alias"))) {
			auto locker = this->get_locker();
			for (auto it = aliass.begin(); it != aliass.end(); ++it) {
				(*it)->release();
			}
			aliass.clear();
			return true;
		}
		if (xml->is_tag(_KS("map"))) {
			auto locker = this->get_locker();
			for (auto it = redirects.begin(); it != redirects.end(); ++it) {
				(*it).second->release();
				xfree((*it).first);
			}
			redirects.clear();
			for (auto it = pathRedirects.begin(); it != pathRedirects.end(); ++it) {
				(*it)->release();
			}
			pathRedirects.clear();
			return true;
		}
		break;
	}
	}
	return false;
}
