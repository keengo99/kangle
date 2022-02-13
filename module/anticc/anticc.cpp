// anticc.cpp : 定义 DLL 应用程序的导出函数。
//
#include "stdafx.h"
#include "anticc.h"
#include "KSessionAccess.h"
static kgl_access anti_access;
static kgl_access session_acl_access;
static kgl_access session_mark_access;
BOOL WINAPI kgl_filter_init(kgl_filter_version * pVer)
{
	pVer->flags = KF_NOTIFY_REQUEST|KF_NOTIFY_END_REQUEST ;
	pVer->filter_version = KGL_FILTER_REVISION;
	char buf[64];
	DWORD size = sizeof(buf)-1;
	if (pVer->get_variable(pVer->ctx,"license_id",buf,&size)!=KGL_OK) {
		return FALSE;
	}

	memset(&anti_access,0,sizeof(anti_access));
	anti_access.name = "anti_cc2";
	anti_access.flags = KF_NOTIFY_REQUEST_MARK;
	anti_access.create_ctx = anticc_create_ctx;
	anti_access.free_ctx = anticc_free_ctx;
	anti_access.build = anticc_build;
	anti_access.parse = anticc_parse;
	anti_access.process = anticc_mark_process;
	pVer->global_support_function(pVer->ctx,KGL_REQ_REGISTER_ACCESS,&anti_access,NULL);
	memset(&session_acl_access,0,sizeof(session_acl_access));
	session_acl_access.name = "session";
	session_acl_access.flags = KF_NOTIFY_REQUEST_ACL|KF_NOTIFY_RESPONSE_ACL;
	session_acl_access.create_ctx = session_create_ctx;
	session_acl_access.free_ctx = session_free_ctx;
	session_acl_access.build = session_build;
	session_acl_access.parse = session_parse;
	session_acl_access.process = session_acl_process;
	pVer->global_support_function(pVer->ctx,KGL_REQ_REGISTER_ACCESS,&session_acl_access,NULL);
	memset(&session_mark_access,0,sizeof(session_mark_access));
	session_mark_access.name = "session";
	session_mark_access.flags = KF_NOTIFY_REQUEST_MARK|KF_NOTIFY_RESPONSE_MARK;
	session_mark_access.create_ctx = session_create_ctx;
	session_mark_access.free_ctx = session_free_ctx;
	session_mark_access.build = session_build;
	session_mark_access.parse = session_parse;
	session_mark_access.process = session_mark_process;
	pVer->global_support_function(pVer->ctx,KGL_REQ_REGISTER_ACCESS,&session_mark_access,NULL);

	return TRUE;

}
DWORD WINAPI kgl_filter_process(kgl_filter_context * ctx,DWORD  type, void * data)
{
	switch (type) {
		case KF_NOTIFY_REQUEST:
			return KF_STATUS_REQ_NEXT_NOTIFICATION;
		case KF_NOTIFY_END_REQUEST:
			{
				anticc_context *ac = (anticc_context *)ctx->pFilterContext;
				if (ac) {
					delete ac;
				}
				ctx->pFilterContext = NULL;
			}
			return KF_STATUS_REQ_NEXT_NOTIFICATION;
	}
	return KF_STATUS_REQ_NEXT_NOTIFICATION ;
}
BOOL  WINAPI kgl_filter_finit(DWORD flag)
{
	return TRUE;
}
