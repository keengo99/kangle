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
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
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
	void get_display(KWStream &s) override {
		s << this->name << " " << total_item;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attr = xml->attr();
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
		KString file;
		if (isAbsolutePath(this->file.c_str())) {
			file = this->file;
		} else {
			file = conf.path + this->file;
		}
		this->load_data(file.c_str());
		kfiber_rwlock_wunlock(lock);
	}
	void get_html(KModel *model,KWStream &s) override {		
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
	}
	KMark * new_instance() override {
		return new KGeoMark;
	}
	const char *getName() override {
		return "geo";
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
	KString file;
	KString name;
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

