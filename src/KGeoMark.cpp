#include <map>
#include <algorithm>
#include "KGeoMark.h"
#include "kselector_manager.h"
#include "KSimulateRequest.h"
#include "KHttpServer.h"
#include "kthread.h"
#define GEO_MARK_TIMER_TICK_MSEC 2000

/* Loaded files by path. The entries hold no ref: release() removes an
 * entry when its last user is gone. */
struct KGeoCache
{
	KGeoCache()
	{
		kmutex_init(&lock, NULL);
	}
	kmutex lock;
	std::map<KString, KGeoData *> files;
};
static KGeoCache &geo_cache()
{
	static KGeoCache cache;
	return cache;
}
KGeoData::KGeoData()
{
	ref = 1;
	last_modified = 0;
	total_item = 0;
	pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
}
KGeoData::~KGeoData()
{
	kgl_destroy_pool(pool);
}
void KGeoData::release()
{
	if (katom_dec((void *)&ref) != 0) {
		return;
	}
	KGeoCache &cache = geo_cache();
	kmutex_lock(&cache.lock);
	auto it = cache.files.find(file);
	if (it != cache.files.end() && (*it).second == this) {
		cache.files.erase(it);
	}
	kmutex_unlock(&cache.lock);
	delete this;
}
KGeoData *KGeoData::cache_get(const KString &file)
{
	KGeoCache &cache = geo_cache();
	KGeoData *d = NULL;
	kmutex_lock(&cache.lock);
	auto it = cache.files.find(file);
	if (it != cache.files.end()) {
		d = (*it).second;
		for (;;) {
			/* a copy whose last ref is being dropped is not reused */
			int32_t ref = (int32_t)katom_get((void *)&d->ref);
			if (ref == 0) {
				d = NULL;
				break;
			}
			if (katom_cas((void *)&d->ref, ref, ref + 1)) {
				break;
			}
		}
	}
	kmutex_unlock(&cache.lock);
	return d;
}
void KGeoData::cache_put()
{
	KGeoCache &cache = geo_cache();
	kmutex_lock(&cache.lock);
	cache.files[file] = this;
	kmutex_unlock(&cache.lock);
}
geo_lable *KGeoData::build_lable(char *str)
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
namespace {
/* one range of the file; seq is the line order */
struct geo_v4_entry {
	uint32_t min;
	uint32_t max;
	uint32_t lable;
	uint32_t seq;
};
struct geo_v6_entry {
	uint32_t min[4];
	uint32_t max[4];
	uint32_t lable;
	uint32_t seq;
};
int v6_cmp(const uint32_t *a, const uint32_t *b)
{
	for (int i = 0; i < 4; i++) {
		if (a[i] != b[i]) {
			return a[i] < b[i] ? -1 : 1;
		}
	}
	return 0;
}
struct geo_v4_traits {
	typedef geo_v4_entry entry;
	static bool by_min(const entry &a, const entry &b)
	{
		return a.min < b.min || (a.min == b.min && a.seq < b.seq);
	}
	static bool by_seq(const entry &a, const entry &b)
	{
		return a.seq < b.seq;
	}
	/* e starts at or before end.max */
	static bool starts_by(const entry &e, const entry &end)
	{
		return e.min <= end.max;
	}
	static bool ends_after(const entry &a, const entry &b)
	{
		return a.max > b.max;
	}
	/* makes e start after prev; false when nothing is left */
	static bool cut(entry &e, const entry &prev)
	{
		if (e.min > prev.max) {
			return true;
		}
		if (prev.max == 0xffffffff) {
			return false;
		}
		e.min = prev.max + 1;
		return e.min <= e.max;
	}
	static void to_ip(const entry &e, ip_addr *lo, ip_addr *hi)
	{
		memset(lo, 0, sizeof(*lo));
		memset(hi, 0, sizeof(*hi));
		lo->sin_family = hi->sin_family = PF_INET;
		lo->addr32[0] = e.min;
		hi->addr32[0] = e.max;
	}
	static void from_ip(const ip_addr *lo, const ip_addr *hi, entry &e)
	{
		e.min = lo->addr32[0];
		e.max = hi->addr32[0];
	}
};
struct geo_v6_traits {
	typedef geo_v6_entry entry;
	static bool by_min(const entry &a, const entry &b)
	{
		int ret = v6_cmp(a.min, b.min);
		return ret < 0 || (ret == 0 && a.seq < b.seq);
	}
	static bool by_seq(const entry &a, const entry &b)
	{
		return a.seq < b.seq;
	}
	static bool starts_by(const entry &e, const entry &end)
	{
		return v6_cmp(e.min, end.max) <= 0;
	}
	static bool ends_after(const entry &a, const entry &b)
	{
		return v6_cmp(a.max, b.max) > 0;
	}
	static bool cut(entry &e, const entry &prev)
	{
		if (v6_cmp(e.min, prev.max) > 0) {
			return true;
		}
		/* e.min = prev.max + 1 */
		bool carry = true;
		for (int i = 3; i >= 0; i--) {
			e.min[i] = prev.max[i] + (carry ? 1 : 0);
			carry = carry && e.min[i] == 0;
		}
		return !carry && v6_cmp(e.min, e.max) <= 0;
	}
	static void to_ip(const entry &e, ip_addr *lo, ip_addr *hi)
	{
		memset(lo, 0, sizeof(*lo));
		memset(hi, 0, sizeof(*hi));
		lo->sin_family = hi->sin_family = PF_INET6;
		kgl_memcpy(lo->addr32, e.min, sizeof(e.min));
		kgl_memcpy(hi->addr32, e.max, sizeof(e.max));
	}
	static void from_ip(const ip_addr *lo, const ip_addr *hi, entry &e)
	{
		kgl_memcpy(e.min, lo->addr32, sizeof(e.min));
		kgl_memcpy(e.max, hi->addr32, sizeof(e.max));
	}
};
/* Sorts v by address. Ranges that overlap no other range stay in v; every
 * group of ranges that overlap each other moves to conflict. */
template<typename T>
void split_overlap(std::vector<typename T::entry> &v, std::vector<typename T::entry> &conflict)
{
	std::sort(v.begin(), v.end(), T::by_min);
	size_t out = 0;
	size_t i = 0;
	while (i < v.size()) {
		size_t end = i;
		size_t j = i + 1;
		while (j < v.size() && T::starts_by(v[j], v[end])) {
			if (T::ends_after(v[j], v[end])) {
				end = j;
			}
			j++;
		}
		if (j - i == 1) {
			v[out++] = v[i];
		} else {
			conflict.insert(conflict.end(), v.begin() + i, v.begin() + j);
		}
		i = j;
	}
	v.resize(out);
}
template<typename T>
void add_to_tree(KIpMap &im, std::vector<typename T::entry> &conflict)
{
	std::sort(conflict.begin(), conflict.end(), T::by_seq);
	ip_addr lo, hi;
	for (auto &&e : conflict) {
		T::to_ip(e, &lo, &hi);
		im.add_range(&lo, &hi, (void *)(uintptr_t)e.lable);
	}
}
struct geo_tree_result {
	std::vector<geo_v4_entry> v4;
	std::vector<geo_v6_entry> v6;
};
iterator_ret geo_tree_dump(void *data, void *arg)
{
	dns_range_addr *addr = (dns_range_addr *)data;
	geo_tree_result *r = (geo_tree_result *)arg;
	uint32_t lable = (uint32_t)(uintptr_t)addr->bind_data;
	if (addr->min_addr.sin_family != addr->max_addr.sin_family) {
		return iterator_continue;
	}
	if (addr->min_addr.sin_family == PF_INET) {
		geo_v4_entry e;
		geo_v4_traits::from_ip(&addr->min_addr, &addr->max_addr, e);
		e.lable = lable;
		e.seq = 0;
		r->v4.push_back(e);
	} else {
		geo_v6_entry e;
		geo_v6_traits::from_ip(&addr->min_addr, &addr->max_addr, e);
		e.lable = lable;
		e.seq = 0;
		r->v6.push_back(e);
	}
	return iterator_continue;
}
/* Leaves v4 and v6 sorted with no overlap. Overlapping ranges go through
 * KIpMap in file order, which resolves them as before (a later range wins
 * inside an earlier one). The groups overlap neither each other nor the
 * other ranges, so resolving only them gives the same result as putting
 * every line in KIpMap, without a tree node for each line. */
void resolve_overlap(std::vector<geo_v4_entry> &v4, std::vector<geo_v6_entry> &v6)
{
	std::vector<geo_v4_entry> c4;
	std::vector<geo_v6_entry> c6;
	split_overlap<geo_v4_traits>(v4, c4);
	split_overlap<geo_v6_traits>(v6, c6);
	if (c4.empty() && c6.empty()) {
		return;
	}
	geo_tree_result r;
	{
		KIpMap im;
		add_to_tree<geo_v4_traits>(im, c4);
		add_to_tree<geo_v6_traits>(im, c6);
		im.dump_addr(geo_tree_dump, &r);
	}
	v4.insert(v4.end(), r.v4.begin(), r.v4.end());
	std::sort(v4.begin(), v4.end(), geo_v4_traits::by_min);
	v6.insert(v6.end(), r.v6.begin(), r.v6.end());
	std::sort(v6.begin(), v6.end(), geo_v6_traits::by_min);
}
}
KGeoData *KGeoData::load(const char *file, time_t last_modified)
{
	KGeoData *d = new KGeoData;
	d->file = file;
	d->last_modified = last_modified;
	std::vector<geo_lable *> lables;
	std::vector<geo_v4_entry> v4;
	std::vector<geo_v6_entry> v6;
	KStreamFile lf;
	if (lf.open(file)) {
		uint32_t seq = 0;
		for (;;) {
			char *line = lf.read();
			if (line == NULL) {
				break;
			}
			/* CRLF files (edited on Windows) kept "\r" in labels and
			 * failed to parse single addresses and min-max ranges */
			char *end = line + strlen(line);
			while (end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
				*--end = '\0';
			}
			if (*line == '*') {
				lables.push_back(d->build_lable(line + 1));
				d->total_item++;
				continue;
			}
			if (lables.empty() || lables.back() == NULL) {
				continue;
			}
			ip_addr lo, hi;
			if (!kgl_parse_ip_range(line, &lo, &hi) || lo.sin_family != hi.sin_family) {
				continue;
			}
			uint32_t lable = (uint32_t)lables.size() - 1;
			if (lo.sin_family == PF_INET) {
				geo_v4_entry e;
				geo_v4_traits::from_ip(&lo, &hi, e);
				e.lable = lable;
				e.seq = seq++;
				if (e.min <= e.max) {
					v4.push_back(e);
				}
				continue;
			}
			geo_v6_entry e;
			geo_v6_traits::from_ip(&lo, &hi, e);
			e.lable = lable;
			e.seq = seq++;
			if (v6_cmp(e.min, e.max) <= 0) {
				v6.push_back(e);
			}
		}
	}
	resolve_overlap(v4, v6);
	d->v4_min.reserve(v4.size());
	d->v4_max.reserve(v4.size());
	d->v4_lable.reserve(v4.size());
	for (size_t i = 0; i < v4.size(); i++) {
		geo_v4_entry e = v4[i];
		/* the tree output may still touch a neighbour: keep lookups exact */
		if (!d->v4_max.empty()) {
			geo_v4_entry prev = { d->v4_min.back(), d->v4_max.back(), 0, 0 };
			if (!geo_v4_traits::cut(e, prev)) {
				continue;
			}
		}
		d->v4_min.push_back(e.min);
		d->v4_max.push_back(e.max);
		d->v4_lable.push_back(lables[e.lable]);
	}
	d->v6.reserve(v6.size());
	for (size_t i = 0; i < v6.size(); i++) {
		geo_v6_entry e = v6[i];
		if (!d->v6.empty()) {
			geo_v6_entry prev;
			kgl_memcpy(prev.min, d->v6.back().min, sizeof(prev.min));
			kgl_memcpy(prev.max, d->v6.back().max, sizeof(prev.max));
			if (!geo_v6_traits::cut(e, prev)) {
				continue;
			}
		}
		v6_range r;
		kgl_memcpy(r.min, e.min, sizeof(r.min));
		kgl_memcpy(r.max, e.max, sizeof(r.max));
		r.lable = lables[e.lable];
		d->v6.push_back(r);
	}
	std::vector<geo_v4_entry>().swap(v4);
	std::vector<geo_v6_entry>().swap(v6);
	size_t count = d->v4_max.size();
	if (count > 0) {
		d->v4_index.resize(65537);
		size_t i = 0;
		for (uint32_t p = 0; p < 65536; p++) {
			uint32_t base = p << 16;
			while (i < count && d->v4_max[i] < base) {
				i++;
			}
			d->v4_index[p] = (uint32_t)i;
		}
		d->v4_index[65536] = (uint32_t)count;
	}
	d->v4_min.shrink_to_fit();
	d->v4_max.shrink_to_fit();
	d->v4_lable.shrink_to_fit();
	d->v6.shrink_to_fit();
	klog(KLOG_NOTICE, "geo file [%s] loaded, %d items, %d ipv4 and %d ipv6 ranges\n",
		file, d->total_item, (int)count, (int)d->v6.size());
	return d;
}
geo_lable *KGeoData::find(const char *ip) const
{
	ip_addr addr;
	if (!ksocket_get_ipaddr(ip, &addr)) {
		return NULL;
	}
	if (addr.sin_family == PF_INET) {
		if (v4_max.empty()) {
			return NULL;
		}
		uint32_t v = ntohl(addr.addr32[0]);
		uint32_t p = v >> 16;
		/* the first range ending at or after v is in [v4_index[p], v4_index[p + 1]] */
		auto begin = v4_max.begin() + v4_index[p];
		auto end = v4_max.begin() + v4_index[p + 1];
		size_t i = std::lower_bound(begin, end, v) - v4_max.begin();
		if (i == v4_max.size() || v4_min[i] > v) {
			return NULL;
		}
		return v4_lable[i];
	}
	if (v6.empty()) {
		return NULL;
	}
	uint32_t v[4];
	for (int i = 0; i < 4; i++) {
		v[i] = ntohl(addr.addr32[i]);
	}
	auto it = std::lower_bound(v6.begin(), v6.end(), v, [](const v6_range &r, const uint32_t *v) {
		return v6_cmp(r.max, v) < 0;
	});
	if (it == v6.end() || v6_cmp((*it).min, v) > 0) {
		return NULL;
	}
	return (*it).lable;
}

static int timer_call_back_fiber(void* arg, int argc)
{
	KGeoMark* gm = (KGeoMark*)arg;
	gm->flush_timer_callback();
	return 0;
}
static kev_result geo_mark_timer(KOPAQUE data, void *arg,int got)
{
	kfiber_create(timer_call_back_fiber, arg, got, http_config.fiber_stack_size, NULL);
	return kev_ok;
}
static kev_result geo_mark_download(KOPAQUE data, void *arg, int status)
{
	KGeoMark *gm = (KGeoMark *)arg;
	gm->download_callback(status);
	return kev_ok;
}
static KTHREAD_FUNCTION geo_load_thread(void *arg)
{
	((KGeoMark *)arg)->load_thread();
	KTHREAD_RETURN;
}
KString KGeoMark::get_path()
{
	if (isAbsolutePath(this->file.c_str())) {
		return this->file;
	}
	return conf.path + this->file;
}
void KGeoMark::set_data(KGeoData *d)
{
	kmutex_lock(&data_lock);
	KGeoData *old = data;
	data = d;
	kmutex_unlock(&data_lock);
	if (old) {
		old->release();
	}
}
void KGeoMark::parse_config(const khttpd::KXmlNodeBody* xml)
{
	auto attr = xml->attr();
	kfiber_rwlock_wlock(lock);
	this->file = attr["file"];
	this->name = attr["name"];
	this->flush_time = atoi(attr["flush_time"].c_str());
	if (this->flush_time <= 0) {
		/* 0 downloaded the file again on every 2 second tick */
		this->flush_time = 86400;
	}
	if (this->url) {
		xfree(this->url);
		this->url = NULL;
	}
	if (!attr["url"].empty()) {
		if (isGlobal) {
			this->url = strdup(attr["url"].c_str());
		} else {
			/* The download is saved to file as root, and a 404 deletes it:
			 * a vhost's own rules could write or delete any file. */
			klog(KLOG_ERR, "geo [%s]: url is only allowed in the global access config, ignored\n", this->name.c_str());
		}
	}
	KString path = get_path();
	time_t last_modified = kfile_last_modified(path.c_str());
	add_flush_timer(last_modified == 0 ? 0 : GEO_MARK_TIMER_TICK_MSEC);
	kfiber_rwlock_wunlock(lock);
	kmutex_lock(&data_lock);
	target_file = path;
	KGeoData *d = data;
	bool same = d && d->file == path && d->last_modified == last_modified;
	kmutex_unlock(&data_lock);
	if (same) {
		return;
	}
	if (last_modified == 0) {
		set_data(NULL);
		return;
	}
	d = KGeoData::cache_get(path);
	if (d) {
		/* a rule edit creates a new mark: reuse the loaded copy, and load
		 * a changed file in the background */
		bool changed = d->last_modified != last_modified;
		set_data(d);
		if (changed) {
			start_load();
		}
		return;
	}
	/* Nothing loaded yet (startup or a new file). Load it now so that the
	 * rules never run without the data. */
	d = KGeoData::load(path.c_str(), last_modified);
	d->cache_put();
	set_data(d);
}
void KGeoMark::start_load()
{
	kmutex_lock(&data_lock);
	if (loading) {
		/* the running load reads the file once more when it is done */
		load_again = true;
		kmutex_unlock(&data_lock);
		return;
	}
	loading = true;
	kmutex_unlock(&data_lock);
	add_ref();
	if (!kthread_pool_start(geo_load_thread, this)) {
		klog(KLOG_ERR, "geo [%s]: cann't start a thread to load the file\n", name.c_str());
		kmutex_lock(&data_lock);
		loading = false;
		load_again = false;
		kmutex_unlock(&data_lock);
		release();
	}
}
/* Loads the file away from the selector threads; requests keep the old
 * data until the new copy is swapped in. */
void KGeoMark::load_thread()
{
	for (;;) {
		time_t loaded = 0;
		kmutex_lock(&data_lock);
		KString path = target_file;
		load_again = false;
		if (data && data->file == path) {
			loaded = data->last_modified;
		}
		kmutex_unlock(&data_lock);
		KGeoData *d = NULL;
		time_t last_modified = kfile_last_modified(path.c_str());
		/* unchanged file (a download that kept the time): nothing to do */
		if (last_modified != 0 && last_modified != loaded) {
			d = KGeoData::load(path.c_str(), last_modified);
			d->cache_put();
		}
		KGeoData *old = NULL;
		kmutex_lock(&data_lock);
		if (d && path == target_file) {
			old = data;
			data = d;
			d = NULL;
		}
		bool again = load_again;
		if (!again) {
			loading = false;
		}
		kmutex_unlock(&data_lock);
		if (d) {
			d->release();
		}
		if (old) {
			old->release();
		}
		if (!again) {
			break;
		}
	}
	release();
}
void KGeoMark::download_callback(int status)
{
	kfiber_rwlock_wlock(lock);
	flush_timer = false;
	if (status == 200) {
		add_flush_timer(GEO_MARK_TIMER_TICK_MSEC);
		kfiber_rwlock_wunlock(lock);
		start_load();
		release();
		return;
	}
	if (status == 404) {
		//remove local
		unlink(get_path().c_str());
		kfiber_rwlock_wunlock(lock);
		set_data(NULL);
		release();
		return;
	}
	add_flush_timer(GEO_MARK_TIMER_TICK_MSEC);
	kfiber_rwlock_wunlock(lock);
	release();
}
void KGeoMark::add_flush_timer(int timer)
{
	if (flush_timer) {
		return;
	}
	flush_timer = true;
	this->add_ref();
	selector_manager_add_timer(geo_mark_timer, this, timer,NULL);
}
void KGeoMark::flush_timer_callback()
{
	if (get_ref() == 1) {
		flush_timer = false;
		this->release();
		return;
	}
	kfiber_rwlock_rlock(lock);
	kassert(flush_timer);
	if (this->url == NULL) {
		flush_timer = false;
		kfiber_rwlock_runlock(lock);
		this->release();
		return;
	}
	if (last_flush <= kgl_current_sec && last_flush + flush_time > kgl_current_sec) {
		kfiber_rwlock_runlock(lock);
		selector_manager_add_timer(geo_mark_timer, this, GEO_MARK_TIMER_TICK_MSEC, NULL);
		return;
	}
	last_flush = kgl_current_sec;
	KString file = get_path();
	char *url = strdup(this->url);
	kfiber_rwlock_runlock(lock);
#ifdef ENABLE_SIMULATE_HTTP
	async_download(url, file.c_str(), geo_mark_download, this);
#else
	geo_mark_download(this, 302);
#endif
	xfree(url);
}
