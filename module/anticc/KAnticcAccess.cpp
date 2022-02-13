#include "KAnticcAccess.h"
#include "KSession.h"
void * WINAPI  anticc_create_ctx()
{
	return new KAnticcAccess;
}
void WINAPI anticc_free_ctx(void *ctx)
{
	KAnticcAccess *a = (KAnticcAccess *)ctx;
	delete a;
}
KGL_RESULT WINAPI  anticc_build(kgl_access_build *build_ctx,enum KF_ACCESS_BUILD_TYPE build_type)
{
	KAnticcAccess *a = (KAnticcAccess *)build_ctx->model_ctx;
	switch (build_type)
	{
	case KF_ACCESS_BUILD_HTML:
		KAnticcAccess::build_html(build_ctx,a);
		break;
	case KF_ACCESS_BUILD_SHORT:
		break;
	case KF_ACCESS_BUILD_XML:
		a->build_xml(build_ctx);
		break;
	}
	return KGL_OK;
}
KGL_RESULT WINAPI  anticc_parse(kgl_access_parse *parse_ctx)
{
	KAnticcAccess *a = (KAnticcAccess *)parse_ctx->model_ctx;
	a->parse(parse_ctx);
	return KGL_OK;
}
DWORD WINAPI anticc_mark_process(kgl_filter_context * ctx,DWORD  type, void * data)
{
	KAnticcAccess *a = (KAnticcAccess *) ctx->pModelContext;
	return a->process(ctx,(kgl_filter_request *)data);
}
DWORD KAnticcAccess::process(kgl_filter_context * ctx,kgl_filter_request *rq)
{
	session_start(ctx);
	if (session_get(ctx,"passed")) {
		return KF_STATUS_REQ_ACCESS_FALSE;
	}
	rq->HttpStatus = 200;
	rq->add_header(ctx,"Content-Type","text/html; charset=utf-8");
	
	return KF_STATUS_REQ_FINISHED;
}
