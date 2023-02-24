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
#ifdef ENABLE_BLACK_LIST
#include "KIpList.h"
#endif
class KApiPipeStream;
class KVirtualHost;
class KHttpFilterManage;
class KVirtualHostEvent
{
public:
	virtual ~KVirtualHostEvent() {
	};
	virtual void setStatus(const char* errMsg) = 0;
	virtual void redirect(const char* event) = 0;
	virtual void buildVh(KVirtualHost* vh) = 0;
	virtual void add(const char* name, const char* value, bool encode = false) = 0;
};
class KBaseString
{
public:
	KBaseString() {
		inherited = false;
	}
	KBaseString(const std::string s) {
		inherited = false;
		this->s = s;
	}
	bool inherited;
	std::string s;
};
struct KIndexItem
{
	int id;
	KBaseString index;
};

class KBaseAlias
{
public:
	KBaseAlias(const char* path, const char* to, const char* doc_root) {
		inherited = false;
		set(path, to, doc_root);
		hit_count = 0;
	}
	KBaseAlias() {
		path = NULL;
		to = NULL;
		inherited = false;
		hit_count = 0;
	}
	~KBaseAlias() {
		if (path) {
			xfree(path);
		}
		if (to) {
			xfree(to);
		}
	}
	/*
	 深度克隆
	 */
	void cloneTo(KBaseAlias* toAlias, const char* doc_root) {
		toAlias->set(orig_path.c_str(), orig_to.c_str(), doc_root);
		toAlias->inherited = inherited;
		toAlias->hit_count = hit_count;
		toAlias->internal = internal;
	}
	bool equalPath(KBaseAlias* a) {
		return filecmp(a->path, path) == 0;
	}
	const char* getPath() {
		return orig_path.c_str();
	}
	const char* getTo() {
		return orig_to.c_str();
	}
	bool match(const char* value, int len) {
		if (len < path_len) {
			return false;
		}
		if (0 == filencmp(value, path, path_len)) {
			hit_count++;
			return true;
		}
		return false;
	}
	char* matched(const char* value, int len) {
		hit_count++;
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
	bool inherited;
	bool internal;
	int hit_count;
	int path_len;
	int to_len;
	char* path;
	char* to;
private:
	void set(const char* path, const char* to, const char* doc_root) {
		orig_path = path;
		orig_to = to;
		this->path = KFileName::tripDir2(path, '/');
		path_len = (int)strlen(this->path);
		if (!isAbsolutePath(to)) {
			this->to = KFileName::concatDir(doc_root, to);
		} else {
			this->to = KFileName::tripDir2(to, PATH_SPLIT_CHAR);
		}
		to_len = (int)strlen(this->to);
	}
	std::string orig_to;
	std::string orig_path;
};
class KBaseVirtualHost
{
public:
	KBaseVirtualHost();
	virtual ~KBaseVirtualHost();
	void swap(KBaseVirtualHost* a);
	std::list<KIndexItem> indexFiles;
	std::map<int, KBaseString> errorPages;
	std::map<char*, KBaseRedirect*, lessf> redirects;
	std::list<KPathRedirect*> pathRedirects;
	KBaseRedirect* defaultRedirect;
	std::list<KBaseAlias*> aliass;
	void getRedirectItemHtml(const std::string& url, const std::string& value, bool file_ext, KBaseRedirect* brd, std::stringstream& s);
	void getIndexHtml(const std::string& url, std::stringstream& s);
	void getErrorPageHtml(const std::string& url, std::stringstream& s);
	void getRedirectHtml(const std::string& url, std::stringstream& s);
	void getAliasHtml(const std::string& url, std::stringstream& s);
	bool delAlias(const char* path);
	bool addAlias(const std::string& path, const std::string& to, const char* doc_root, bool internal, int id, std::string& errMsg);
	bool addIndexFile(const std::string& index, int id = 100);
	void listIndex(KVirtualHostEvent* ev);
	//bool delIndexFile(size_t index);
	bool delIndexFile(const std::string& index);
	void getParsedFileExt(KVirtualHostEvent* ctx);
	bool addRedirect(bool file_ext, const std::string& value, KRedirect* rd, const std::string& allowMethod, uint8_t confirmFile, const std::string& params);
	bool addRedirect(bool file_ext, const std::string& value, const std::string& target, const std::string& allowMethod, uint8_t confirmFile, const std::string& params);
	bool delRedirect(bool file_ext, const std::string& value);
	bool addErrorPage(int code, const std::string& url);
	bool delErrorPage(int code);
	bool getErrorPage(int code, std::string& errorPage);
	bool alias(bool internal, const char* path, KFileName* file, bool& exsit, int flag);
	char* alias(bool internal, const char* path);
	bool getIndexFile(KHttpRequest* rq, KFileName* file, KFileName** newFile, char** newPath);
	void buildBaseXML(std::stringstream& s);
	/**
	* 继承给vh,clearFlag标识是否先清除继承的设置再继承(用于重新继承)
	*/
	void inheriTo(KVirtualHost* vh, bool clearFlag = false);
	void changeInherit(bool remove);
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
	void getMimeTypeHtml(const std::string& url, std::stringstream& s);
	friend class KNsVirtualHost;
	friend class KHttpServerParser;
	friend class KVirtualHostManage;
	/*
	 克隆基本设置到vh.
	 copyInherit 是否复制继承的设置
	 changeInherit 复制设置时inherit改变情况，2=不改变，0=变成不继承，1=变成继承
	 */
	void cloneTo(KVirtualHost* vh, bool copyInherit, int changeInherit);
	/*
		变量
	*/
	std::map<char*, char*, lessp_icase> envs;
	bool getEnvValue(const char* name, std::string& value);
	bool addEnvValue(const char* name, const char* value);
	void buildEnv(std::stringstream& s) {
		std::map<char*, char*, lessp_icase>::iterator it5;
		for (it5 = envs.begin(); it5 != envs.end(); it5++) {
			s << (*it5).first << "='" << (*it5).second << "' ";
		}
	}
	void parseEnv(const char* str) {
		std::map<char*, char*, lessp_icase> attr;
		char* buf = strdup(str);
		buildAttribute(buf, attr);
		std::map<char*, char*, lessp_icase>::iterator it;
		for (it = attr.begin(); it != attr.end(); it++) {
			addEnvValue((*it).first, (*it).second);
		}
		free(buf);
	}
	void getErrorEnv(const char* split, KStringBuf& s);
	void getIndexFileEnv(const char* split, KStringBuf& s);
	KMimeType* mimeType;
#ifdef ENABLE_BLACK_LIST
	KIpList* blackList;
#endif
private:
	void copyTo(KVirtualHost* vh, bool copyInherit, int changeInherit);
	/*
	 改变 继承变量,remove是否删除继承.
	 */
	void internalChangeInherit(bool remove);
	void clearEnv();
protected:
	KReMutex lock;
};
#endif
