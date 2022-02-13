#include <map>
#include "KDsoAsyncFetchObject.h"
#include "KAccessDsoSupport.h"
#include "KDsoRedirect.h"
#include "KDsoNextUpstream.h"
#include "http.h"
#include "KCdnContainer.h"


kgl_async_context * kgl_get_async_context(KCONN cn)
{
	KDsoAsyncFetchObject *fo = (KDsoAsyncFetchObject *)cn;
	return &fo->ctx;
}
static KGL_RESULT support_function (
	KREQUEST r,
	KCONN      cn,
	KF_REQ_TYPE  req,
	PVOID    data,
	PVOID *ret)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	return base_support_function(rq, req, data, ret);
}
static KGL_RESULT open_next(KREQUEST r, KCONN cn, kgl_input_stream* in, kgl_output_stream* out, const char* queue)
{
	KDsoAsyncFetchObject* fo = (KDsoAsyncFetchObject*)cn;
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->OpenNextFetchObject(fo, in, out, queue);
}
static kgl_async_context_function  async_context_function = {
	(kgl_get_variable_f)get_request_variable,
	open_next,
	NULL,
	support_function
};

KDsoAsyncFetchObject::~KDsoAsyncFetchObject()
{
	if (ctx.module) {
		kassert(brd!=NULL);
		GetAsyncUpstream()->free_ctx(ctx.module);
	}
}
void KDsoAsyncFetchObject::init()
{
	ctx.cn = this;	
	ctx.f = &async_context_function;
	if (ctx.module == NULL) {
		ctx.module = GetAsyncUpstream()->create_ctx();
	}
}
KGL_RESULT KDsoAsyncFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{	
	if (ctx.f == NULL) {
		init();
	}
	ctx.in = in;
	ctx.out = out;
	return GetAsyncUpstream()->open(rq, &ctx);
}
uint32_t KDsoAsyncFetchObject::Check(KHttpRequest *rq)
{
	if (ctx.f == NULL) {
		init();
	}	
	kgl_async_upstream *async_us = GetAsyncUpstream();
	if (async_us->check) {
		ctx.in = NULL;
		ctx.out = get_check_output_stream();
		return CheckResult(rq, async_us->check(rq, &ctx));
	}
	return KFetchObject::Check(rq);
}
