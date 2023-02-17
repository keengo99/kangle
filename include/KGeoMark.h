#ifndef KGEOMARK_H
#define KGEOMARK_H
#include "KMark.h"
#include "krbtree.h"
#include "KIpMap.h"
#include "utils.h"
#include "KLineFile.h"
#include "kfiber_sync.h"

struct geo_lable {
	char *data;
	int len;
	geo_lable *next;
};
class KGeoMark : public KMark
{
public:
	KGeoMark()
	{
		last_modified = 0;
		last_flush = 0;
		pool = NULL;
		total_item = 0;
		url = NULL;
		flush_time = 86400;
		flush_timer = false;
		lock = kfiber_rwlock_init();
	}
	~KGeoMark()
	{
		clean_env();
		if (url) {
			xfree(url);
		}
		kassert(flush_timer == false);
		kfiber_rwlock_destroy(lock);
	}
	bool supportRuntime() override
	{
		return false;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override {
		kfiber_rwlock_rlock(lock);
		geo_lable *lable = (geo_lable *)im.find(rq->getClientIp());
		if (lable == NULL) {
			kfiber_rwlock_runlock(lock);
			return false;
		}
		while (lable) {
			auto header = rq->sink->data.add_header(this->name.c_str(), (int)this->name.size(), lable->data, lable->len);
			header->name_is_internal = 1;
			lable = lable->next;
		}
		kfiber_rwlock_runlock(lock);
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << this->name << " " << total_item;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attr, bool html)	override {
		kfiber_rwlock_wlock(lock);
		this->file = attr["file"];
		this->name = attr["name"];
		this->flush_time = atoi(attr["flush_time"].c_str());
		if (this->url) {
			xfree(this->url);
			this->url = NULL;
		}
		if (!attr["url"].empty()) {
			this->url = strdup(attr["url"].c_str());
		}
		std::string file;
		if (isAbsolutePath(this->file.c_str())) {
			file = this->file;
		} else {
			file = conf.path + this->file;
		}
		this->load_data(file.c_str());
		kfiber_rwlock_wunlock(lock);
	}
	bool startCharacter(KXmlContext *context, char *character, int len) override {
		return true;
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		KGeoMark *m = (KGeoMark *)model;
		s << "name:<input name='name' value='";
		if (m) {
			s << m->name;
		}
		s << "'>";
		s << "file:<input name='file' value='";
		if (m) {
			s << m->file;
		}
		s << "'>";
		s << "url:<input name='url' value='";
		if (m && m->url) {
			s << m->url;
		}
		s << "'>";
		s << "flush_time:<input name='flush_time' value='";
		if (m) {
			s << m->flush_time;
		}
		s << "'>";
		return s.str();
	}
	KMark * new_instance() override {
		return new KGeoMark;
	}
	const char *getName() override {
		return "geo";
	}
	void buildXML(std::stringstream &s) override {
		s << " file='" << this->file << "'";
		s << " name='" << this->name << "'";
		if (url) {
			int url_len = (int)strlen(url);
			char *encode_url = KXml::htmlEncode(url, url_len, NULL);
			s << " url='" << encode_url << "'";
			xfree(encode_url);
			s << " flush_time='" << flush_time << "'";
		}
		s << ">";
	}
	void flush_timer_callback();
	void download_callback(int status);
private:
	void load_data(const char *file);
	void add_flush_timer(int timer);
	geo_lable *build_lable(char *str)
	{
		geo_lable *last = NULL;
		while (*str) {
			while (*str && *str == '*') {
				str++;
			}
			char *end = str;
			while (*end && *end != '*') {
				end++;
			}
			int len = (int)(end - str);
			if (*end) {
				*end++ = '\0';
			}			
			if (len > 0) {
				geo_lable *lable = (geo_lable *)kgl_pnalloc(pool, sizeof(geo_lable));
				lable->data = (char *)kgl_pnalloc(pool, len);
				lable->len = len;
				kgl_memcpy(lable->data, str, len);
				lable->next = last;
				last = lable;
			}
			str = end;
		}
		return last;
	}
	void clean_env()
	{
		if (pool) {
			kgl_destroy_pool(pool);
			pool = NULL;
		}
		total_item = 0;
	}
	std::string file;
	std::string name;
	char *url;
	KIpMap im;
	kgl_pool_t *pool;
	time_t last_modified;
	time_t last_flush;
	int total_item;
	int flush_time;
	bool flush_timer;
	kfiber_rwlock* lock;
	//KRWLock lock;
};
#endif

