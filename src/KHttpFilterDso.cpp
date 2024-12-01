#include "kforwin32.h"
#include "KDynamicString.h"
#include "log.h"
#include "KAccess.h"
#include "KAccessDso.h"
#include "kselector_manager.h"
#include "KTimer.h"
#include "kmalloc.h"
#include "KAccessDsoSupport.h"
#include "KSimulateRequest.h"
#include "extern.h"
#ifdef ENABLE_KSAPI_FILTER
KGL_RESULT global_get_variable(PVOID ctx, KGL_GVAR type, const char *name, LPVOID value, LPDWORD size)
{
	KDsoExtend* dso = (KDsoExtend*)ctx;
	switch (type) {
	case KGL_GBASE_PATH:
		return add_api_var(value, size, conf.path.c_str(), (int)conf.path.size());
	case KGL_GNAME:
	{
		const char* val = getSystemEnv(name);
		if (val == NULL || *val == '\0') {
			return KGL_EINVALID_PARAMETER;
		}
		return add_api_var(value, size, val);
	}
	case KGL_GCONFIG:
	{
		auto it = dso->attribute.find(name);
		if (it == dso->attribute.end()) {
			return KGL_ENO_DATA;
		}
		return add_api_var(value, size, (*it).second.c_str(), (int)(*it).second.size());
	}
	default:
		return KGL_ENOT_SUPPORT;
	}
}
static kev_result timer_fiber(KOPAQUE data, void* arg, int got)
{
	kfiber_create((kfiber_start_func)data, arg, got, 0, NULL);
	return kev_ok;
}
KGL_RESULT global_support_function(
	PVOID                        ctx,
	DWORD                        req,
	PVOID                        data,
	PVOID* ret
)
{
	switch (req) {
	case KGL_REQ_REGISTER_ACCESS: {
		kgl_access* access = (kgl_access*)data;
		if (KBIT_TEST(access->flags, KF_NOTIFY_REQUEST_ACL)) {
			KAccessDso* model = new KAccessDso(access,
				(KDsoExtend*)ctx,
				KF_NOTIFY_REQUEST_ACL);
			KAccess::addAclModel(REQUEST, new KAccessDsoAcl(model), KBIT_TEST(access->flags, KF_NOTIFY_REPLACE) > 0);
		}
		if (KBIT_TEST(access->flags, KF_NOTIFY_RESPONSE_ACL)) {
			KAccessDso* model = new KAccessDso(access,
				(KDsoExtend*)ctx,
				KF_NOTIFY_RESPONSE_ACL);
			KAccess::addAclModel(RESPONSE, new KAccessDsoAcl(model), KBIT_TEST(access->flags, KF_NOTIFY_REPLACE) > 0);
		}
		if (KBIT_TEST(access->flags, KF_NOTIFY_REQUEST_MARK)) {
			KAccessDso* model = new KAccessDso(access,
				(KDsoExtend*)ctx,
				KF_NOTIFY_REQUEST_MARK);
			KAccess::addMarkModel(REQUEST, new KAccessDsoMark(model), KBIT_TEST(access->flags, KF_NOTIFY_REPLACE) > 0);
		}
		if (KBIT_TEST(access->flags, KF_NOTIFY_RESPONSE_MARK)) {
			KAccessDso* model = new KAccessDso(access,
				(KDsoExtend*)ctx,
				KF_NOTIFY_RESPONSE_MARK);
			KAccess::addMarkModel(RESPONSE, new KAccessDsoMark(model), KBIT_TEST(access->flags, KF_NOTIFY_REPLACE) > 0);
		}
		return KGL_OK;
	}
	case KGL_REQ_REGISTER_UPSTREAM:
	{
		kgl_upstream* us = (kgl_upstream*)data;
		KDsoExtend* de = (KDsoExtend*)ctx;
		if (de->RegisterUpstream(us)) {
			return KGL_OK;
		}
		return KGL_EUNKNOW;
	}
	case KGL_REQ_SERVER_VAR: {
		const char* name = (const char*)data;
		if (name == NULL) {
			return KGL_EINVALID_PARAMETER;
		}
		const char* val = getSystemEnv(name);
		if (val == NULL || *val == '\0') {
			return KGL_EINVALID_PARAMETER;
		}
		//(*ret) = (void *)strdup(val);
		return KGL_OK;
	}
	case KGL_REQ_MODULE_SHUTDOWN:
	{
		int32_t* value = (int32_t*)data;
		return KGL_OK;
	}
#ifdef ENABLE_SIMULATE_HTTP
	case KGL_REQ_ASYNC_HTTP_UPSTREAM:
	{
		kgl_http_upstream* ctx = (kgl_http_upstream*)data;
		KHttpRequest* rq = kgl_create_simulate_request(&ctx->http_ctx);
		if (rq == NULL) {
			return KGL_EINVALID_PARAMETER;
		}
		KDsoRedirect* rd = new KDsoRedirect("", ctx->us);
		KRedirectSource* fo = rd->makeFetchObject(rq, ctx->us_ctx);
		fo->bind_base_redirect(new KBaseRedirect(rd, KConfirmFile::Never));
		//KBIT_SET(ctx->us->flags, KGL_UPSTREAM_FINAL_SOURCE);
		rq->append_source(fo);
		return (KGL_RESULT)kgl_simuate_start_as_new_fiber(rq, (kfiber **)ret);
	}
	case KGL_REQ_ASYNC_HTTP:
	{
		kgl_async_http* ctx = (kgl_async_http*)data;
		if (kgl_simuate_http_request(ctx, (kfiber**)ret) == 0) {
			return KGL_OK;
		}
		return KGL_EINVALID_PARAMETER;
	}
#endif
	case KGL_REQ_TIMER_FIBER:
	case KGL_REQ_TIMER: {
		kgl_timer* t = (kgl_timer*)data;
		if (!is_selector_manager_init()) {
			return KGL_ENOT_READY;
		}
		kselector* selector = kgl_get_tls_selector();
		if (selector == NULL || selector->aysnc_main) {
			selector = get_perfect_selector();
		}
		if (req == KGL_REQ_TIMER_FIBER) {
			kselector_add_timer_ts(selector, timer_fiber, t->arg, t->msec, (KOPAQUE)t->data);
		}
		else {
			kselector_add_timer_ts(selector, t->cb, t->arg, t->msec, NULL);
		}
		return KGL_OK;
	}
	case KGL_REQ_CREATE_WORKER: {
		int* max_worker = (int*)data;
		kasync_worker* worker = kasync_worker_init(*max_worker, 0);
		*ret = (void*)worker;
		return KGL_OK;
	}
	case KGL_REQ_RELEASE_WORKER: {
		kasync_worker* worker = (kasync_worker*)data;
		kasync_worker_release(worker);
		return KGL_OK;
	}
	case KGL_REQ_THREAD: {
		kgl_thread* thread = (kgl_thread*)data;
		if (thread->worker) {
			kasync_worker* worker = (kasync_worker*)thread->worker;
			if (!kasync_worker_try_start(worker, thread->param, thread->thread_function, false)) {
				return KGL_EUNKNOW;
			}
			return KGL_OK;
		}
		if (kasync_worker_thread_start(thread->param, thread->thread_function)) {
			return KGL_OK;
		}
		return KGL_EUNKNOW;
	}
	case KGL_REQ_REGISTER_VARY: {
		kgl_vary* vary = (kgl_vary*)data;
		if (register_vary_extend(vary)) {
			return KGL_OK;
		}
		return KGL_EUNKNOW;
	}
#ifndef NDEBUG
	case KGL_REQ_SHUTDOWN_SINK: {
		/* only for test code */
		KHttpRequest* rq = (KHttpRequest*)data;
		rq->sink->shutdown();
		return KGL_OK;
	}
#endif
	}
	return KGL_EINVALID_PARAMETER;
}
#endif
