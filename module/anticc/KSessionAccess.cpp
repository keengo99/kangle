#include "KSessionAccess.h"
#include "KSession.h"
void * WINAPI  session_create_ctx()
{
	KSessionAccess *sa = new KSessionAccess;
	return sa;
}
void WINAPI session_free_ctx(void *ctx)
{
	KSessionAccess *sa = (KSessionAccess *)ctx;
	delete sa;
}
KGL_RESULT WINAPI  session_build(kgl_access_build *build_ctx,enum KF_ACCESS_BUILD_TYPE build_type)
{
	KSessionAccess *a = (KSessionAccess *)build_ctx->model_ctx;
	switch (build_type) {
		case KF_ACCESS_BUILD_HTML:
			KSessionAccess::build_html(build_ctx,a);
			break;
		case KF_ACCESS_BUILD_SHORT:
			a->build_short(build_ctx);
			break;
		case KF_ACCESS_BUILD_XML:
			a->build_xml(build_ctx);
			break;
	}
	return KGL_OK;
}
KGL_RESULT WINAPI  session_parse(kgl_access_parse *parse_ctx)
{
	KSessionAccess *a = (KSessionAccess *)parse_ctx->model_ctx;
	a->parse(parse_ctx);
	return KGL_OK;
}
DWORD WINAPI session_acl_process(kgl_filter_context * ctx,DWORD  type, void * data)
{
	session_start(ctx);
	KSessionAccess *a = (KSessionAccess *)ctx->pModelContext;
	if (a->name) {
		const char *value = session_get(ctx,a->name);
		if (value==NULL) {
			return KF_STATUS_REQ_ACCESS_FALSE;
		}
		return KF_STATUS_REQ_ACCESS_TRUE;
	}
	return KF_STATUS_REQ_ACCESS_FALSE;
}
DWORD WINAPI session_mark_process(kgl_filter_context * ctx,DWORD  type, void * data)
{
	session_start(ctx);
	KSessionAccess *a = (KSessionAccess *)ctx->pModelContext;
	if (a->name) {
		if (a->value==NULL || *a->value=='\0') {
			session_del(ctx,a->name);
		} else {
			session_set(ctx,a->name,a->value);
		}
	}
	return KF_STATUS_REQ_ACCESS_TRUE;
}