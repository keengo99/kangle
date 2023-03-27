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
	decltype (redirects.find((char*)"")) it;
	auto locker = get_locker();
	if (!file_ext) {
		it = redirects.find((char*)"*");
	} else {
		it = redirects.find((char*)file_ext);
		if (it == redirects.end()) {
			it = redirects.find((char*)"*");
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
bool KBaseVirtualHost::getErrorPage(int code, KString& errorPage) {
	{
		auto locker = get_locker();
		auto it = errorPages.find(code);
		if (it != errorPages.end()) {
			errorPage = (*it).second;
			return true;
		} else {
			it = errorPages.find(0);
			if (it != errorPages.end()) {
				errorPage = (*it).second;
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
			if ((*newFile)->setName(file->getName(), (*it).c_str(), rq->getFollowLink()) && !(*newFile)->isDirectory()) {
				(*newFile)->setIndex((*it).c_str());
				if (newPath) {
					assert(*newPath == NULL);
					*newPath = KFileName::concatDir(rq->sink->data.url->path, (*it).c_str());
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
	KStringBuf s;
	lock.Lock();
	for (auto it = indexFiles.begin(); it != indexFiles.end(); it++) {
		s << "<index file='" << (*it) << "'/>\n";
	}
	lock.Unlock();
	ev->add("indexs", s.str().c_str(), false);
}
void KBaseVirtualHost::getIndexHtml(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>id</td><td>file</td></tr>";
	int j = 0;
	for (auto it = indexFiles.begin(); it != indexFiles.end(); ++it, ++j) {
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=indexdelete&index=" << j;
		s << "&" << url << "';}\">" << LANG_DELETE << "</a>]";
		s << "</td>";
		s << "<td>" << j << "</td>";
		s << "<td>" << (*it) << "</td>";
		s << "</tr>";
	}
	s << "</table>";
	s << "<form action='/vhbase?action=indexadd&" << url << "' method='post'>";
	s << klang["id"] << ":<input name='index' value='-1' size=4>";
	s << klang["index"] << ":<input name='file'><input type='submit' value='" << LANG_ADD << "'>";
	s << "</form>";
}
void KBaseVirtualHost::getErrorPageHtml(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
		<< klang["error_code"];
	s << "</td><td>" << klang["error_url"] << "</td></tr>\n";
	for (auto it = errorPages.begin(); it != errorPages.end(); it++) {
		s << "<tr><td>";

		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=errorpagedelete&code=";
		s << (*it).first << "&" << url << "';}\">" << LANG_DELETE << "</a>]";

		s << "</td>";
		s << "<td>" << (*it).first << "</td>";
		s << "<td>" << (*it).second << "</td>";
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
void KBaseVirtualHost::getMimeTypeHtml(const KString& url, KWStream& s) {
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
void KBaseVirtualHost::getAliasHtml(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
		<< klang["id"] << "</td><td>" << klang["url_path"] << "</td>";
	s << "<td>" << klang["phy_path"] << "</td>";
	s << "<td>" << klang["internal"] << "</td>";
	s << "</tr>\n";
	int i = 1;
	for (auto it = aliass.begin(); it != aliass.end(); it++) {
		s << "<tr><td>";

		s
			<< "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=aliasdelete&path=";
		s << (*it)->get_path() << "&" << url << "';}\">" << LANG_DELETE
			<< "</a>]";

		s << "</td>";
		s << "<td>";
		s << i;
		i++;
		s << "</td>";
		s << "<td>" << (*it)->get_path() << "</td>";
		s << "<td>" << (*it)->get_to() << "</td>";
		s << "<td>" << ((*it)->internal ? klang["internal"] : "&nbsp;") << "</td>";
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
void KBaseVirtualHost::getRedirectItemHtml(const KString& url, const KString& value, int index, bool file_ext, KBaseRedirect* brd, KWStream& s) {
	s << "<tr><td>";
	s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=redirectdelete&type=";
	s << (file_ext ? "file_ext" : "path");
	s << "&index=" << index;
	s << "&value=";
	s << url_encode(url_encode(value.c_str()).c_str());
	s << "&" << url << "';}\">" << LANG_DELETE << "</a>]";
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
	s << "<td>";
	brd->allowMethod.getMethod(s);
	s << "</td>";
	s << "<td>" << kgl_get_confirm_file_tips(brd->confirm_file) << "</td>";
#ifdef ENABLE_UPSTREAM_PARAM
	s << "<td>";
	brd->buildParams(s);
	s << "</td>";
#endif
	s << "</tr>";
}
void KBaseVirtualHost::getRedirectHtml(const KString& url, KWStream& s) {
	s << "<table border=1><tr><td>"
		<< LANG_OPERATOR << "</td><td>"
		<< klang["map_type"] << "</td><td>"
		<< LANG_VALUE << "</td><td>"
		<< klang["extend"] << "</td><td>"
		<< klang["allow_method"] << "</td><td>"
		<< klang["confirm_file"] << "</td>";
#ifdef ENABLE_UPSTREAM_PARAM
	s << "<td>" << klang["params"] << "</td>";
#endif
	s << "</tr>";
	int index = 0;
	for (auto it2 = pathRedirects.begin(); it2 != pathRedirects.end(); it2++) {
		getRedirectItemHtml(url, (*it2)->path, index, false, (*it2), s);
		++index;
	}
	index = 0;
	for (auto it = redirects.begin(); it != redirects.end(); it++) {
		getRedirectItemHtml(url, (*it).first, index, true, (*it).second, s);
		++index;
	}
	s << "</table>";
	s << "<form action='/vhbase?action=redirectadd&" << url << "' method='post'>";
	s << "<table border=0><tr><td>";
	s << "<select name='type'>";
	s << "<option value='file_ext'>" << klang["file_ext"] << "</option>";
	s << "<option value='path'>" << klang["path"] << "</option>";
	s << "</select>";
	s << "<input name='value'>";
	s << "</td></tr>";
	s << "<tr><td>";
	auto targets = conf.gam->getAllTarget();
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
bool KBaseVirtualHost::addRedirect(bool file_ext, const KString& value, KRedirect* rd, const KString& allowMethod, KConfirmFile confirmFile, const KString& params) {
	auto locker = get_locker();
	if (file_ext) {
		auto it = redirects.find((char*)value.c_str());
		if (it != redirects.end()) {
			(*it).second->release();
			redirects.erase(it);
		}
		KBaseRedirect* nrd = new KBaseRedirect(rd, confirmFile);
		nrd->allowMethod.setMethod(allowMethod.c_str());
#ifdef ENABLE_UPSTREAM_PARAM
		nrd->parseParams(params);
#endif
		redirects.insert(pair<char*, KBaseRedirect*>(xstrdup(value.c_str()), nrd));
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
bool KBaseVirtualHost::addRedirect(bool file_ext, const KString& value, const KString& extend, const KString& allowMethod, KConfirmFile confirmFile, const KString& params) {

	KRedirect* rd = NULL;
	if (extend != "default") {
		rd = conf.gam->refsRedirect(extend);
		if (rd == NULL) {
			return false;
		}
	}
	return addRedirect(file_ext, value, rd, allowMethod, confirmFile, params);
}
bool KBaseVirtualHost::addErrorPage(int code, const KString& url) {
	lock.Lock();
	errorPages[code] = url;
	lock.Unlock();
	return true;
}
bool KBaseVirtualHost::delErrorPage(int code) {
	bool result = false;
	lock.Lock();
	auto it = errorPages.find(code);
	if (it != errorPages.end()) {
		errorPages.erase(it);
		result = true;
	}
	lock.Unlock();
	return result;
}
bool KBaseVirtualHost::addAlias(const KString& path, const KString& to, const char* doc_root, bool internal, int id,
	KString& errMsg) {
	if (path.empty() || to.empty()) {
		errMsg = "path or to cann't be zero";
		return false;
	}
	if (path[0] != '/') {
		errMsg = "path not start with `/` char";
		return false;
	}
	KAlias alias(new KBaseAlias(path.c_str(), to.c_str(), internal));
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
void KBaseVirtualHost::getParsedFileExt(KVirtualHostEvent* ctx) {
	lock.Lock();
	for (auto it2 = redirects.begin(); it2 != redirects.end(); it2++) {
		ctx->add("file_ext", (*it2).first.c_str());
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
bool KBaseVirtualHost::getEnvValue(const char* name, KString& value) {
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
		s << (*it2).first << "=" << (*it2).second;
	}
	lock.Unlock();
}
void KBaseVirtualHost::getIndexFileEnv(const char* split, KStringBuf& s) {
	lock.Lock();
	for (auto it = indexFiles.begin(); it != indexFiles.end(); it++) {
		if (s.size() > 0) {
			s << split;
		}
		s << (*it);
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
	if (!rd && attr["extend"] != "default") {
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
		static_cast<KVirtualHost*>(bvh)->release();
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
			std::list<KIndexItem> indexs;
			for (auto&& body : xml->body) {
				auto attr2 = body->attributes;
				indexs.push_back(attr2["file"]);
			}
			auto locker = get_locker();
			this->indexFiles.swap(indexs);
			return true;
		}
		if (xml->is_tag(_KS("map_file"))) {
			auto locker = get_locker();
			auto attr2 = xml->attributes();
			auto file_ext = attr2("ext", nullptr);
			if (file_ext != nullptr) {
				auto it = redirects.find((char*)file_ext);
				if (it != redirects.end()) {
					(*it).second->release();
					redirects.erase(it);
				}
				auto rd = parse_file_map(attr2);
				if (rd) {
					redirects.emplace(file_ext, rd);
				}
			}
			return true;
		}
		if (xml->is_tag(_KS("map_path"))) {
			std::list<KPathRedirect*> path_maps;
			defer(
				for (auto it = path_maps.begin(); it != path_maps.end(); ++it) {
					(*it)->release();
				});
			for (auto&& body : xml->body) {
				auto attr2 = body->attributes;
				auto rd = parse_path_map(attr2);
				if (rd) {
					path_maps.push_back(rd);
				}
			}
			{
				auto locker = get_locker();
				path_maps.swap(pathRedirects);
			}

			return true;
		}
		if (xml->is_tag(_KS("alias"))) {
			std::list<KAlias> alias;
			for (auto&& body : xml->body) {
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
		if (xml->is_tag(_KS("map_file"))) {
			auto locker = this->get_locker();
			auto it = redirects.find(attr["ext"]);
			if (it != redirects.end()) {
				(*it).second->release();
				redirects.erase(it);
			}
			return true;
		}
		if (xml->is_tag(_KS("map_path"))) {
			auto locker = this->get_locker();
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
