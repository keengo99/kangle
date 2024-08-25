#include "KDsoExtend.h"
#include "KConfig.h"
#include "extern.h"
#include "KDynamicString.h"
#include "klog.h"
#include "kselector_manager.h"
#include "KDsoAsyncFetchObject.h"
#include "KCdnContainer.h"
#include "KAsyncFileApi.h"
#include "KSocketApi.h"
#include "kfiber.h"
#include "kfiber_sync.h"
extern kgl_http_object_function http_object_provider;
static KFIBER_COND kgl_kfiber_cond_init(bool thread_safe, bool auto_reset) {
	if (thread_safe) {
		return kfiber_cond_init_ts(auto_reset);
	}
	return kfiber_cond_init(auto_reset);
}
static int kgl_fiber_wait(void* obj) {
	return __kfiber_wait(kfiber_self2(), obj);
}
static void kgl_fiber_wakeup(KFIBER fiber, void* obj, int ret) {
	kfiber_wakeup((kfiber*)fiber, obj, ret);
}
static int kgl_kfiber_cond_wait(KFIBER_COND cond, int* got) {
	return ((kfiber_cond*)cond)->f->wait((kfiber_cond*)cond, got);
}
static int kgl_kfiber_cond_notice(KFIBER_COND cond, int got) {
	return ((kfiber_cond*)cond)->f->notice((kfiber_cond*)cond, got);
}
static void kgl_kfiber_cond_destroy(KFIBER_COND cond) {
	((kfiber_cond*)cond)->f->release((kfiber_cond*)cond);
}
static const char* get_know_header(kgl_header_type header, hlen_t* len) {
	*len = (int)kgl_header_type_string[header].value.len;
	return kgl_header_type_string[header].value.data;
}
static const char* get_header_name(KHttpHeader* header, hlen_t* len) {
	if (header->name_is_know) {
		*len = (hlen_t)kgl_header_type_string[header->know_header].value.len;
		return kgl_header_type_string[header->know_header].value.data;
	}
	*len = header->name_len;
	return header->buf;
}
static void next_call(KSELECTOR selector, KOPAQUE data, result_callback result, void* arg, int got) {
	kgl_selector_module.next((kselector*)selector, data, result, arg, got);
}
static void* alloc_memory(KREQUEST r, int  size, KF_ALLOC_MEMORY_TYPE memory_type) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (memory_type == KF_ALLOC_REQUEST) {
		return rq->alloc_request_memory(size);
	}
	return rq->alloc_connect_memory(size);
}
static KGL_RESULT register_clean_callback(KREQUEST r, kgl_cleanup_f cb, void* arg, KF_ALLOC_MEMORY_TYPE type) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (type == KF_ALLOC_REQUEST) {
		rq->registerRequestCleanHook(cb, arg);
	} else {
		rq->registerConnectCleanHook(cb, arg);
	}
	return KGL_OK;
}
static int get_selector_index() {
	kselector* selector = kgl_get_tls_selector();
	if (selector == NULL) {
		return -1;
	}
	return selector->sid;
}
KDsoExtend::KDsoExtend(const char* name) {
	this->name = xstrdup(name);
	filename = NULL;
	orign_filename = NULL;
	memset(&version, 0, sizeof(version));
}
KDsoExtend::~KDsoExtend() {
	xfree(name);
	if (filename) {
		xfree(filename);
	}
	if (orign_filename) {
		xfree(orign_filename);
	}
}
bool KDsoExtend::RegisterUpstream(kgl_upstream* us) {
	//global upstream always open after cache handle.
	KBIT_CLR(us->flags, KGL_UPSTREAM_BEFORE_CACHE);
	std::map<const char*, KDsoRedirect*, lessp>::iterator it;
	it = upstream.find(us->name);
	if (it != upstream.end()) {
		return false;
	}
	upstream.insert(std::pair<const char*, KDsoRedirect*>(us->name, new KDsoRedirect(name, us)));
	return true;
}
KRedirect* KDsoExtend::RefsRedirect(KString& name) {
	std::map<const char*, KDsoRedirect*, lessp>::iterator it;
	it = upstream.find(name.c_str());
	if (it == upstream.end()) {
		return NULL;
	}
	KRedirect* rd = (*it).second;
	rd->add_ref();
	return rd;
}
void KDsoExtend::ListUpstream(KWStream& s) {
	std::map<const char*, KDsoRedirect*, lessp>::iterator it;
	for (it = upstream.begin(); it != upstream.end(); it++) {
		s << (*it).first << " ";
	}
}
void KDsoExtend::ListTarget(std::vector<KString>& target) {
	std::map<const char*, KDsoRedirect*, lessp>::iterator it;
	for (it = upstream.begin(); it != upstream.end(); it++) {
		std::stringstream s;
		s << "dso:" << this->name << ":" << (*it).first;
		target.push_back(s.str());
	}
}
static kgl_dso_function dso_function = {
	global_support_function,
	global_get_variable,
	get_selector_count,
	get_selector_index,
	(KSELECTOR(*)())kgl_get_tls_selector,
	(KSELECTOR(*)())get_perfect_selector,
	(bool(*)(KSELECTOR))kselector_is_same_thread,
	next_call,
	kgl_get_async_context,
	alloc_memory,
	register_clean_callback,
	klog,
	get_header_name,
	get_know_header,
	(bool (*)(KGL_POOL, KHttpHeader* , const char* , int))kgl_build_know_header_value,
	NULL,
	kgl_parse_response_header
		};
static kgl_mutex_function mutex_function = {
	(KFIBER_MUTEX(*)(int))kfiber_mutex_init2,
	(void (*)(KFIBER_MUTEX , int))kfiber_mutex_set_limit,
	(int (*)(KFIBER_MUTEX))kfiber_mutex_lock,
	(int (*)(KFIBER_MUTEX ,int))kfiber_mutex_try_lock,
	(int (*)(KFIBER_MUTEX))kfiber_mutex_unlock,
	(void (*)(KFIBER_MUTEX))kfiber_mutex_destroy
};
static kgl_cond_function cond_function = {
	kgl_kfiber_cond_init,
	kgl_kfiber_cond_wait,
	kgl_kfiber_cond_notice,
	kgl_kfiber_cond_destroy
};
static kgl_kfiber_function fiber_function = {
	(int (*) (kgl_fiber_start_func , void* , int , int , KFIBER*))kfiber_create,
	(int (*) (KSELECTOR, kgl_fiber_start_func , void* , int , int , KFIBER*))kfiber_create2,
	(int (*)(KFIBER , int*))kfiber_join,
	(int (*)(int))kfiber_msleep,
	(KFIBER(*)())kfiber_self2,
	(KFIBER(*)(bool))kfiber_ref_self,
	(void (*)(KFIBER, void*, int))kgl_fiber_wakeup,
	(int (*)(void*))kgl_fiber_wait,
	(int (*)(kgl_fiber_start_func, void*, int, int*))kfiber_thread_call
};
static kgl_chan_function chan_function = {
	(KFIBER_CHAN(*)())kfiber_chan_create,
	(int (*)(KFIBER_CHAN , KOPAQUE))kfiber_chan_send,
	(int (*)(KFIBER_CHAN , KOPAQUE*, KFIBER*))kfiber_chan_recv,
	(int (*)(KFIBER_CHAN)) kfiber_chan_close,
	(void (*)(KFIBER_CHAN, KFIBER, int))kfiber_chan_wakeup,
	(KFIBER_CHAN(*)(KFIBER_CHAN))kfiber_chan_add_ref,
	(int (*)(KFIBER_CHAN))kfiber_chan_get_ref,
	(void (*)(KFIBER_CHAN))kfiber_chan_release
};
static KGL_POOL get_connection_pool(KREQUEST rq) {
	return (KGL_POOL)((KHttpRequest*)rq)->sink->get_connection_pool();
}
static KGL_POOL get_request_pool(KREQUEST rq) {
	return (KGL_POOL)((KHttpRequest*)rq)->sink->pool;
}
static kgl_pool_function pool_function = {
	get_connection_pool,
	get_request_pool,
	(void* (*)(KGL_POOL , size_t))kgl_palloc,
	(void* (*)(KGL_POOL , size_t))kgl_pnalloc,
	(void* (*)(KGL_POOL , size_t , size_t))kgl_pmemalign,
	(kgl_cleanup_t * (*)(KGL_POOL , kgl_cleanup_f , void*))kgl_cleanup_add,
	(kgl_cleanup_t * (*)(KGL_POOL , kgl_cleanup_f))kgl_cleanup_insert,
	(void* (*)(kgl_cleanup_t*))kgl_cleanup_get_data,
	(void  (*)(kgl_cleanup_t* , void*))kgl_cleanup_set_data
};
bool KDsoExtend::load(const char* filename, const KXmlAttribute& attribute) {
	this->attribute = attribute;
	orign_filename = strdup(filename);
	KDynamicString ds;
	this->filename = ds.parseString(filename).release();
	if (!dso.load(this->filename)) {
		klog(KLOG_ERR, "cann't load dso extend [%s]\n", name);
		return false;
	}
	kgl_dso_init = (kgl_dso_init_f)dso.findFunction("kgl_dso_init");
	if (kgl_dso_init == NULL) {
		klog(KLOG_ERR, "cann't load dso extend [%s] kgl_dso_init function cann't find\n", name);
		return false;
	}
	kgl_dso_finit = (kgl_dso_finit_f)dso.findFunction("kgl_dso_finit");
	memset(&version, 0, sizeof(version));
	version.api_version = KSAPI_VERSION;
	version.f = &dso_function;
	version.file = &async_file_provider;
	version.socket_client = &tcp_socket_provider;
	version.obj = &http_object_provider;
	version.fiber = &fiber_function;
	version.mutex = &mutex_function;
	version.cond = &cond_function;
	version.chan = &chan_function;
	version.pool = &pool_function;
	version.cn = this;
	if (!kgl_dso_init(&version)) {
		klog(KLOG_ERR, "cann't load dso extend [%s] kgl_dso_init return false\n", name);
		return false;
	}
	if (!IS_KSAPI_VERSION_COMPATIBLE(version.api_version)) {
		klog(KLOG_ERR, "cann't load dso extend [%s] module api version=[%d.%d] not compatible\n", name, HIWORD(version.api_version), LOWORD(version.api_version));
		return false;
	}
	return true;
}
void KDsoExtend::shutdown() {
	if (kgl_dso_finit) {
		kgl_dso_finit(0);
	}
}
