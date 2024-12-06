#ifndef KBASEVIRTUALHOST_H
#define KBASEVIRTUALHOST_H
#include <vector>
#include <map>
#include <string>
#include "KRedirect.h"
#include "KPathRedirect.h"
#include "KMutex.h"
#include "utils.h"
#include "KFileName.h"
#include "global.h"
#include "KXml.h"
#include "KContentType.h"
#include "KPathHandler.h"
#include "KConfigTree.h"
#include "KPathHandler.h"
#include "KSharedObj.h"
#ifdef ENABLE_BLACK_LIST
#include "KIpList.h"
#endif
void on_vh_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
class KApiPipeStream;
class KVirtualHost;
class KHttpFilterManage;
class KDataAttribute
{
public:
	virtual ~KDataAttribute() {
	};
	virtual bool add(const KString &name, const KString& value, bool encode = false) = 0;
	virtual bool add(const KString& name, int64_t v) = 0;
	virtual KDataAttribute* add(const KString& name) = 0;
	virtual void build(KWStream& s, int format) = 0;
};
class KVirtualHostEvent 
{
public:
	virtual ~KVirtualHostEvent() {
	};
	virtual KDataAttribute* data() = 0;
	virtual void setStatus(const char* errMsg) = 0;
	virtual void redirect(const char* event) = 0;
	virtual void buildVh(KVirtualHost* vh) = 0;
};
using KIndexItem = KString;
using KBaseString = KString;
class KBaseAlias
{
public:
	KBaseAlias(const char* path, const char* to, bool internal) {
		set(path, to);
		ref = 1;
		this->internal = internal;
	}
#if 0
	KBaseAlias() {
		path = NULL;
		to = NULL;
		inherited = false;
		hit_count = 0;
		ref = 1;
	}

	/*
	 ��ȿ�¡
	 */
	void cloneTo(KBaseAlias* toAlias, const char* doc_root) {
		toAlias->set(orig_path.c_str(), orig_to.c_str(), doc_root);
		toAlias->inherited = inherited;
		toAlias->hit_count = hit_count;
		toAlias->internal = internal;
	}
#endif
	bool same_path(const KBaseAlias* a) const {
		return filecmp(a->path, path) == 0;
	}
	const char* get_path() const {
		return orig_path.c_str();
	}
	KBaseAlias* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	const char* get_to() const {
		return orig_to.c_str();
	}
	bool match(const char* value, int len) {
		if (len < path_len) {
			return false;
		}
		if (0 == filencmp(value, path, path_len)) {
			return true;
		}
		return false;
	}
	char* matched(const char* value, int len) {
		assert(len >= path_len);
		int left_len = len - path_len;
		int new_len = left_len + to_len;
		char* new_value = (char*)xmalloc(new_len + 1);
		new_value[new_len] = '\0';
		kgl_memcpy(new_value, to, to_len);
		if (left_len <= 0) {
			return new_value;
		}
		kgl_memcpy(new_value + to_len, value + path_len, left_len);
		return new_value;
	}
	char* path;
	char* to;
	bool internal;
	int path_len;
	int to_len;
private:
	volatile uint32_t ref;
	void set(const char* path, const char* to) {
		orig_path = path;
		orig_to = to;
		this->path = KFileName::tripDir2(path, '/');
		path_len = (int)strlen(this->path);
		this->to = KFileName::tripDir2(to, PATH_SPLIT_CHAR);
#if 0
		if (!kgl_is_absolute_path(to)) {
			this->to = KFileName::concatDir(doc_root, to);
		} else {
			this->to = KFileName::tripDir2(to, PATH_SPLIT_CHAR);
		}
#endif
		to_len = (int)strlen(this->to);
	}
	~KBaseAlias() {
		if (path) {
			xfree(path);
		}
		if (to) {
			xfree(to);
		}
	}
	KString orig_to;
	KString orig_path;
};
using KAlias = KSharedObj<KBaseAlias>;
class KBaseVirtualHost : public kconfig::KConfigListen
{
public:
	KBaseVirtualHost();
	virtual ~KBaseVirtualHost();
	virtual bool is_global() {
		return true;
	}
	KAlias find_alias(bool internal, const char* path, size_t path_len);
	KBaseRedirect* refs_path_redirect(const char* path, int path_len);
	KFetchObject* find_path_redirect(KHttpRequest* rq, KFileName* file, const char* path, size_t path_len, bool fileExsit, bool& result);
	KFetchObject* find_file_redirect(KHttpRequest* rq, KFileName* file, const char* file_ext, bool fileExsit, bool& result);
	virtual bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	virtual kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir | kconfig::ev_self;
	}
	void swap(KBaseVirtualHost* a);
	std::list<KIndexItem> indexFiles;
	std::map<int, KBaseString> errorPages;
	std::map<KString, KBaseRedirect*, kstring_lessf> redirects;
	std::list<KPathRedirect*> pathRedirects;
	std::list<KAlias> aliass;
	void getRedirectItemHtml(const KString& url, const KString& value,int index, bool file_ext, KBaseRedirect* brd, KWStream& s);
	void getIndexHtml(const KString& url, KWStream& s);
	void getErrorPageHtml(const KString& url, KWStream& s);
	void getRedirectHtml(const KString& url, KWStream& s);
	void getAliasHtml(const KString& url, KWStream& s);
	bool addAlias(const KString& path, const KString& to, const char* doc_root, bool internal, int id, KString& errMsg);
	//bool addIndexFile(const KString& index);
	void listIndex(KVirtualHostEvent* ev);
	//bool delIndexFile(const KString& index);
	void getParsedFileExt(KVirtualHostEvent* ctx);
	bool addRedirect(bool file_ext, const KString& value, KRedirect* rd, const KString& allowMethod, KConfirmFile confirmFile, const KString& params);
	bool addRedirect(bool file_ext, const KString& value, const KString& target, const KString& allowMethod, KConfirmFile confirmFile, const KString& params);
	bool addErrorPage(int code, const KString& url);
	bool delErrorPage(int code);
	bool getErrorPage(int code, KString& errorPage);
	bool getIndexFile(KHttpRequest* rq, KFileName* file, KFileName** newFile, char** newPath);
	char* getMimeType(KHttpObject* obj, const char* ext) {
		char* type = NULL;
		lock.Lock();
		if (mimeType) {
			type = mimeType->get(obj, ext);
		}
		lock.Unlock();
		return type;
	}
	void addMimeType(const char* ext, const char* type, kgl_compress_type compress, int max_age);
	bool delMimeType(const char* ext);
	void getMimeTypeHtml(const KString& url, KWStream& s);
	friend class KHttpServerParser;
	friend class KVirtualHostManage;
	/*
		����
	*/
	std::map<char*, char*, lessp_icase> envs;
	bool getEnvValue(const char* name, KString& value);
	bool addEnvValue(const char* name, const char* value);
	void buildEnv(KWStream& s) {
		for (auto it5 = envs.begin(); it5 != envs.end(); ++it5) {
			s << (*it5).first << "='" << (*it5).second << "' ";
		}
	}
	void parseEnv(const char* str) {
		std::map<char*, char*, lessp_icase> attr;
		char* buf = strdup(str);
		buildAttribute(buf, attr);
		for (auto it = attr.begin(); it != attr.end(); ++it) {
			addEnvValue((*it).first, (*it).second);
		}
		free(buf);
	}
	void getErrorEnv(const char* split, KStringBuf& s);
	void getIndexFileEnv(const char* split, KStringBuf& s);
	void clear();
	KMimeType* mimeType;
#ifdef ENABLE_BLACK_LIST
	KIpList* blackList;
#endif
	KLocker get_locker() {
		return KLocker(&lock);
	}
	KBaseRedirect* parse_file_map(const KXmlAttribute& attr);
	KPathRedirect* parse_path_map(const KXmlAttribute& attr);
	KAlias parse_alias(const KXmlAttribute& attr);

protected:
	KMutex lock;
};
#endif
