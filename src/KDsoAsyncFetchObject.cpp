#include <map>
#include "KDsoAsyncFetchObject.h"
#include "KAccessDsoSupport.h"
#include "KDsoRedirect.h"
#include "KDsoNextUpstream.h"
#include "http.h"
#include "KCdnContainer.h"


kgl_async_context* kgl_get_async_context(KCONN cn) {
	KDsoAsyncFetchObject* fo = (KDsoAsyncFetchObject*)cn;
	return &fo->ctx;
}
static void readhup(KREQUEST r) {
	((KHttpRequest*)r)->readhup();
}
static KGL_RESULT support_function(
	KREQUEST     r,
	KCONN        cn,
	KF_REQ_TYPE  req,
	PVOID        data,
	PVOID* ret) {
	KHttpRequest* rq = (KHttpRequest*)r;
	return base_support_function(rq, req, data, ret);
}
static KGL_RESULT open_next(KREQUEST r, KCONN cn, kgl_input_stream* in, kgl_output_stream* out, const char* queue) {
	KDsoAsyncFetchObject* fo = (KDsoAsyncFetchObject*)cn;
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->open_next(fo, in, out, queue);
}
static KGL_RESULT replace_next(KREQUEST r, KCONN cn, const char* us) {
	KDsoAsyncFetchObject* fo = (KDsoAsyncFetchObject*)cn;
	KHttpRequest* rq = (KHttpRequest*)r;
	auto next_fo = server_container->get(us);
	if (next_fo == nullptr) {
		return KGL_EUNKNOW;
	}
	auto old_next_fo = rq->replace_next(fo, next_fo);
	while (old_next_fo) {
		auto next = old_next_fo->next;
		delete old_next_fo;
		old_next_fo = next;
	}
	return KGL_OK;
}
static kgl_async_context_function  async_context_function = {
	(kgl_get_variable_f)get_request_variable,
	open_next,
	replace_next,
	support_function,
	readhup
};

KDsoAsyncFetchObject::~KDsoAsyncFetchObject() {
	if (ctx.module) {
		kassert(brd != NULL);
		get_upstream()->free_ctx(ctx.module);
	}
}
void KDsoAsyncFetchObject::init() {
	ctx.cn = this;
	ctx.f = &async_context_function;
	if (ctx.module == NULL) {
		ctx.module = get_upstream()->create_ctx();
	}
}
void KDsoAsyncFetchObject::on_readhup(KHttpRequest* rq) {
	if (ctx.f) {
		kgl_upstream* us = get_upstream();
		if (us->on_readhup) {
			us->on_readhup(rq, &ctx);
		}
	}
}
KGL_RESULT KDsoAsyncFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	if (ctx.f == NULL) {
		init();
	}
	ctx.in = in;
	ctx.out = out;
	return get_upstream()->open(rq, &ctx);
}
