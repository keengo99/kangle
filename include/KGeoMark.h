#ifndef KGEOMARK_H
#define KGEOMARK_H
#include <vector>
#include "KMark.h"
#include "KIpMap.h"
#include "utils.h"
#include "KLineFile.h"
#include "kfiber_sync.h"
#include "ksync.h"
#include "klog.h"

struct geo_lable {
	char *data;
	int len;
	geo_lable *next;
};
/* The ranges of one geo file. It does not change after load(), so lookups
 * take no lock. Marks that use the same unchanged file share one copy. */
class KGeoData
{
public:
	/* Reads file into a new object (ref 1); it is not in the cache yet. */
	static KGeoData *load(const char *file, time_t last_modified);
	/* The cached copy of file (one more ref), or NULL. */
	static KGeoData *cache_get(const KString &file);
	/* Makes this the copy cache_get returns for its file. */
	void cache_put();
	geo_lable *find(const char *ip) const;
	void add_ref() {
		katom_inc((void *)&ref);
	}
	void release();
	KString file;
	time_t last_modified;
	int total_item;
private:
	KGeoData();
	~KGeoData();
	geo_lable *build_lable(char *str);
	volatile uint32_t ref;
	kgl_pool_t *pool;
	/* IPv4 ranges in host byte order, sorted and not overlapping:
	 * range i is v4_min[i]..v4_max[i] */
	std::vector<uint32_t> v4_min;
	std::vector<uint32_t> v4_max;
	std::vector<geo_lable *> v4_lable;
	/* v4_index[p] is the first range whose v4_max is not below p << 16 */
	std::vector<uint32_t> v4_index;
	struct v6_range {
		uint32_t min[4];
		uint32_t max[4];
		geo_lable *lable;
	};
	std::vector<v6_range> v6;
};
class KGeoMark : public KMark
{
public:
	KGeoMark()
	{
		last_flush = 0;
		url = NULL;
		flush_time = 86400;
		flush_timer = false;
		lock = kfiber_rwlock_init();
		data = NULL;
		loading = false;
		load_again = false;
		kmutex_init(&data_lock, NULL);
	}
	~KGeoMark()
	{
		if (data) {
			data->release();
		}
		if (url) {
			xfree(url);
		}
		kassert(flush_timer == false);
		kassert(loading == false);
		kfiber_rwlock_destroy(lock);
		kmutex_destroy(&data_lock);
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KGeoData *d = get_data();
		if (d == NULL) {
			return KF_STATUS_REQ_FALSE;
		}
		geo_lable *lable = d->find(rq->getClientIp());
		if (lable == NULL) {
			d->release();
			return KF_STATUS_REQ_FALSE;
		}
		while (lable) {
			auto header = rq->sink->data.add_header(this->name.c_str(), (int)this->name.size(), lable->data, lable->len);
			if (header) {
				header->name_is_internal = 1;
			}
			lable = lable->next;
		}
		d->release();
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream &s) override {
		KGeoData *d = get_data();
		s << this->name << " " << (d ? d->total_item : 0);
		if (d) {
			d->release();
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
	void get_html(KWStream &s) override {
		KGeoMark *m = (KGeoMark *)this;
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
	const char *get_module() const override {
		return "geo";
	}
	void flush_timer_callback();
	void download_callback(int status);
	void load_thread();
private:
	KString get_path();
	KGeoData *get_data()
	{
		kmutex_lock(&data_lock);
		KGeoData *d = data;
		if (d) {
			d->add_ref();
		}
		kmutex_unlock(&data_lock);
		return d;
	}
	/* takes over the caller's ref of d */
	void set_data(KGeoData *d);
	void start_load();
	void add_flush_timer(int timer);
	KString file;
	KString name;
	char *url;
	time_t last_flush;
	int flush_time;
	bool flush_timer;
	/* config fields and the flush timer, used in fibers */
	kfiber_rwlock* lock;
	/* data and the background load, used by requests and the load thread */
	kmutex data_lock;
	KGeoData *data;
	/* the file this mark uses now; a load of another file is dropped */
	KString target_file;
	bool loading;
	bool load_again;
};
#endif
