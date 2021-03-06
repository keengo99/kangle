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
static void next_call(KSELECTOR selector, KOPAQUE data, result_callback result, void *arg, int got)
{
	kgl_selector_module.next((kselector *)selector, data, result, arg, got);
}
static void *alloc_memory(KREQUEST r, DWORD  cbSize, KF_ALLOC_MEMORY_TYPE memory_type)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	if (memory_type == KF_ALLOC_REQUEST) {
		return rq->alloc_request_memory(cbSize);
	}
	return rq->alloc_connect_memory(cbSize);
}
static KGL_RESULT register_clean_callback(KREQUEST r, kgl_cleanup_f cb, void *arg, KF_ALLOC_MEMORY_TYPE type)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	if (type == KF_ALLOC_REQUEST) {
		rq->registerRequestCleanHook(cb, arg);
	} else {
		rq->registerConnectCleanHook(cb, arg);
	}
	return KGL_OK;
}
static int get_selector_index()
{
	kselector *selector = kgl_get_tls_selector();
	if (selector == NULL) {
		return -1;
	}
	return selector->sid;
}
KDsoExtend::KDsoExtend(const char *name)
{
	this->cur_config_ext = ::cur_config_ext;
	this->name = xstrdup(name);
	filename = NULL;
	orign_filename = NULL;
	memset(&version, 0, sizeof(version));
}
KDsoExtend::~KDsoExtend()
{
	xfree(name);
	if (filename) {
		xfree(filename);
	}
	if (orign_filename) {
		xfree(orign_filename);
	}
}
bool KDsoExtend::RegisterUpstream(kgl_upstream *us)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	it = upstream.find(us->name);
	if (it != upstream.end()) {
		return false;
	}
	upstream.insert(std::pair<const char *, KDsoRedirect *>(us->name,new KDsoRedirect(name,us)));
	return true;
}
KRedirect *KDsoExtend::RefsRedirect(std::string &name)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	it = upstream.find(name.c_str());
	if (it == upstream.end()) {
		return NULL;
	}
	KRedirect *rd = (*it).second;
	rd->addRef();
	return rd;
}
void KDsoExtend::ListUpstream(std::stringstream &s)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
	for (it = upstream.begin(); it != upstream.end(); it++) {
		s << (*it).first << " ";
	}
}
void KDsoExtend::ListTarget(std::vector<std::string> &target)
{
	std::map<const char *, KDsoRedirect *, lessp>::iterator it;
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
	(KSELECTOR (*)())kgl_get_tls_selector,
	(KSELECTOR(*)())get_perfect_selector,
	(bool(*)(KSELECTOR))kselector_is_same_thread,
	next_call,
	kgl_get_async_context,
	alloc_memory,
	register_clean_callback,
	klog
};
static kgl_kfiber_function fiber_function = {
	(int (*) (kgl_fiber_start_func , void* , int , int , KFIBER*))kfiber_create,
	(int (*) (KSELECTOR, kgl_fiber_start_func , void* , int , int , KFIBER*))kfiber_create2,
	(int (*)(KFIBER , int* ))kfiber_join,
	(int (*)(int))kfiber_msleep,
	(KFIBER(*)())kfiber_self,
	(KFIBER(*)(bool))kfiber_ref_self,
	(void (*)(KFIBER, void*, int))kfiber_wakeup,
	(int (*)(void*))kfiber_wait,
	(KFIBER_MUTEX(*)(int ))kfiber_mutex_init2,
	(void (*)(KFIBER_MUTEX , int))kfiber_mutex_set_limit,
	(int (*)(KFIBER_MUTEX ))kfiber_mutex_lock,
	(int (*)(KFIBER_MUTEX ,int ))kfiber_mutex_try_lock,
	(int (*)(KFIBER_MUTEX ))kfiber_mutex_unlock,
	(void (*)(KFIBER_MUTEX ))kfiber_mutex_destroy
};
bool KDsoExtend::load(const char *filename, std::map<std::string, std::string> &attribute)
{
	this->attribute = attribute;
	orign_filename = strdup(filename);
	KDynamicString ds;
	this->filename = ds.parseString(filename);
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
	version.cn = this;
	if (!kgl_dso_init(&version)) {
		klog(KLOG_ERR, "cann't load dso extend [%s] kgl_dso_init return false\n", name);
		return false;
	}
	if (!IS_KSAPI_VERSION_COMPATIBLE(version.api_version)) {
		klog(KLOG_ERR, "cann't load dso extend [%s] module api version=[%d.%d] not compatible\n", name, HIWORD(version.api_version),LOWORD(version.api_version));
		return false;
	}
	return true;
}
void KDsoExtend::shutdown()
{
	if (kgl_dso_finit) {
		kgl_dso_finit(0);
	}
}
