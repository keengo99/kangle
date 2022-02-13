#include "kforwin32.h"
#include "KHttpFilterContext.h"
#include "KHttpRequest.h"
#include "KHttpFilterDsoManage.h"
#include "KSubVirtualHost.h"
#include "kselector_manager.h"
#include "ssl_utils.h"
//{{ent
#include "KAttackRequestManager.h"
//}}
#include "http.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0

static KGL_RESULT WINAPI  ServerSupportFunction (
	kgl_filter_context * pfc,
	enum KF_REQ_TYPE     kfReq,
	PVOID                pData,
	DWORD                ul1,
	DWORD                ul2
	)
{
#if 0
	assert(pfc->ulReserved>=0 && (int)pfc->ulReserved < KHttpFilterDsoManage::cur_dso_index);
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	switch (kfReq) {
	case KF_REQ_DISABLE_NOTIFICATIONS:
		{
			rq->http_filter_ctx->filterContext[pfc->ulReserved].disabled_flags |= ul1;
			return KGL_OK;
		}
	case KF_REQ_CONNECT_CLEAN:
		{
			kgl_call_back *c = (kgl_call_back *)pData;
			rq->registerConnectCleanHook(c->call_back,c->arg);
			return KGL_OK;
		}
	case KF_REQ_REQUEST_CLEAN:
		{
			kgl_call_back *c = (kgl_call_back *)pData;
			rq->registerRequestCleanHook(c->call_back,c->arg);
			return KGL_OK;
		}
	case KF_REQ_REWRITE_URL:
		{
			const char *url = (const char *)pData;
			if (rq->rewriteUrl(url)) {
				return KGL_OK;
			}
			return KGL_EINVALID_PARAMETER;
		}
		/*
	case KF_REQ_SAVE_POST: {
			const char *ctx = (const char *)pData;
			if (ctx == NULL) {
				return KGL_EINVALID_PARAMETER;
			}
			INT64 left_read = rq->left_read - rq->parser.bodyLen;
			if (left_read>0) {
				if (rq->content_length>conf.max_post_size) {
					return KGL_EINSUFFICIENT_BUFFER;
				}
				if (rq->tf) {
					return KGL_EUNKNOW;
				}
				rq->tf = new KTempFile;
				rq->tf->init(left_read);
				int size = strlen(ctx);
				char *arg = (char *)rq->alloc_request_memory(size + 1);
				kgl_memcpy(arg, ctx, size + 1);
				if (rq->tf->post_ctx == NULL) {
					rq->tf->post_ctx = new KTempFilePostContext;
				}
				rq->tf->post_ctx->arg = arg;
				//rq->tf->startPost(rq, AfterPostForAttack, arg);
				return KGL_OK;
			}
			return KGL_EUNKNOW;
		}
		//{{ent
#ifdef ENABLE_FATBOY
	case KF_REQ_RESTORE_POST: {								  
		const char *ctx = (const char *)pData;
		if (ctx == NULL) {
			return KGL_EINVALID_PARAMETER;
		}
		attackRequestManager.loadRequest(ctx, rq);
		return KGL_OK;
	}
#endif//}}
	*/
	default:
		break;
	}
#endif
	return KGL_EUNKNOW;
}
#endif
#if 0
KHttpFilterContext::KHttpFilterContext()
{
	memset(&ctx,0,sizeof(ctx));
	filterContext = NULL;
}
KHttpFilterContext::~KHttpFilterContext()
{
	if (filterContext) {
		xfree(filterContext);
	}
}
//static kgl_filter_context global_http_filter_context;
void KHttpFilterContext::init(KHttpRequest *rq)
{
	ctx.ServerContext = (void *)rq;
	ctx.cbSize = sizeof(ctx);
	ctx.alloc_memory = AllocMem;
	ctx.add_headers = AddResponseHeaders;
	ctx.get_variable = GetServerVariable;
	ctx.support_function = ServerSupportFunction;
	ctx.write_client = WriteClient;
	//if (TEST(rq->workModel,WORK_MODEL_SSL)) {
	//	ctx.fIsSecurePort = TRUE;
	//}
	assert(filterContext==NULL);
	assert(KHttpFilterDsoManage::cur_dso_index>0);
	int filterContextSize = sizeof(KHttpFilterRequestContext)*KHttpFilterDsoManage::cur_dso_index;
	filterContext = (KHttpFilterRequestContext *)xmalloc(filterContextSize);
	memset(filterContext,0,filterContextSize);
}
#endif
#endif
