#include "KGeoMark.h"
#include "kselector_manager.h"
#include "KSimulateRequest.h"
#include "KHttpServer.h"
#define GEO_MARK_TIMER_TICK_MSEC 2000
int timer_call_back_fiber(void* arg, int argc)
{
	KGeoMark* gm = (KGeoMark*)arg;
	gm->flush_timer_callback();
	return 0;
}
kev_result geo_mark_timer(KOPAQUE data, void *arg,int got)
{	
	kfiber_create(timer_call_back_fiber, arg, got, http_config.fiber_stack_size, NULL);
	return kev_ok;
}
kev_result geo_mark_download(KOPAQUE data, void *arg, int status)
{
	KGeoMark *gm = (KGeoMark *)arg;
	gm->download_callback(status);
	return kev_ok;
}
void KGeoMark::download_callback(int status)
{
	kfiber_rwlock_wlock(lock);
	flush_timer = false;
	KString file;
	if (isAbsolutePath(this->file.c_str())) {
		file = this->file;
	} else {
		file = conf.path + this->file;
	}
	if (status == 200) {
		load_data(file.c_str());
		kfiber_rwlock_wunlock(lock);
		release();
		return;
	}
	if (status == 404) {
		//remove local
		unlink(file.c_str());
		im.clear();
		clean_env();
	} else {
		add_flush_timer(GEO_MARK_TIMER_TICK_MSEC);
	}
	kfiber_rwlock_wunlock(lock);
	release();
	return;
}
void KGeoMark::load_data(const char *file)
{	
	time_t last_modified = kfile_last_modified(file);
	if (last_modified == 0) {
		add_flush_timer(0);
	} else {
		add_flush_timer(GEO_MARK_TIMER_TICK_MSEC);
	}
	if (last_modified == this->last_modified) {
		return;
	}
	this->last_modified = last_modified;
	im.clear();
	clean_env();
	pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	KStreamFile lf;
	if (!lf.open(file)) {
		return;
	}
	geo_lable *current_lable = NULL;
	for (;;) {
		char *line = lf.read();
		if (line == NULL) {
			break;
		}
		if (*line == '*') {
			current_lable = build_lable(line + 1);
			total_item++;
			continue;
		}
		if (current_lable == NULL) {
			continue;
		}
		im.add_addr(line, current_lable);
	}
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
	KString file;
	if (isAbsolutePath(this->file.c_str())) {
		file = this->file;
	} else {
		file = conf.path + this->file;
	}
	char *url = strdup(this->url);
	kfiber_rwlock_runlock(lock);
#ifdef ENABLE_SIMULATE_HTTP
	async_download(this->url, file.c_str(), geo_mark_download, this);
#else
	geo_mark_download(this, 302);
#endif
	xfree(url);
}
