#include "KHttpFilterHookCollectResponse.h"
#include "KHttpRequest.h"
#include "http.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0
static KGL_RESULT WINAPI  GetHeader (
    kgl_filter_context * pfc,
    LPSTR                         lpszName,
    LPVOID                        lpvBuffer,
    LPDWORD                       lpdwSize)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	KHttpObject *obj = rq->ctx->obj;
	if (obj==NULL) {
		return KGL_EUNKNOW;
	}
	KHttpHeader *header = obj->data->headers;
	while (header) {
		if (is_attr(header,lpszName)) {
			DWORD len = strlen(header->val) + 1;
			*lpdwSize = MIN(*lpdwSize,len);
			kgl_memcpy(lpvBuffer,header->val,*lpdwSize);
			return KGL_OK;
		}
		header = header->next;
	}
	return KGL_ENO_DATA;
}
static KGL_RESULT WINAPI  SetHeader(kgl_filter_context* pfc,
    LPSTR                         lpszName,
    LPSTR                         lpszValue)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	KHttpObject *obj = rq->ctx->obj;
	if (obj==NULL) {
		return KGL_EUNKNOW;
	}
	KHttpHeader *header = obj->data->headers;
	while (header) {
		if (is_attr(header,lpszName)) {
			xfree(header->val);
			header->val = xstrdup(lpszValue);
			header->val_len = (hlen_t)strlen(lpszValue);
			return KGL_OK;
		}
		header = header->next;
	}
	return KGL_ENO_DATA;
}
static KGL_RESULT WINAPI  AddHeader (
    kgl_filter_context * pfc,
    LPSTR                         lpszName,
    LPSTR                         lpszValue)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	KHttpObject *obj = rq->ctx->obj;
	if (obj==NULL) {
		return KGL_EUNKNOW;
	}
	KHttpHeader *new_t = (struct KHttpHeader *) xmalloc(sizeof(KHttpHeader));
	if (new_t == NULL) {
		return KGL_ENO_DATA;
	}
	new_t->attr = xstrdup(lpszName);
	new_t->val = xstrdup(lpszValue);
	new_t->attr_len = (hlen_t)strlen(lpszName);
	new_t->val_len = (hlen_t)strlen(lpszValue);
	new_t->next = obj->data->headers;
	obj->data->headers = new_t;
	return KGL_OK;
}
int KHttpFilterHookCollectResponse::check_response(KHttpRequest *rq)
{
	kgl_filter_response notification;
	init_kgl_filter_response(notification);
	KHttpObject *obj = rq->ctx->obj;
	notification.HttpStatus = obj->data->status_code;
	DWORD ret = process(rq,KF_NOTIFY_RESPONSE,&notification);
	obj->data->status_code = (unsigned short)notification.HttpStatus;
	switch (ret) {
	case KF_STATUS_REQ_HANDLED_NOTIFICATION:
	case KF_STATUS_REQ_NEXT_NOTIFICATION:
	case KF_STATUS_REQ_READ_NEXT:
		return JUMP_ALLOW;
	default:
		return JUMP_DENY;
	}
}
void init_kgl_filter_response(kgl_filter_response &notification)
{
	memset(&notification,0,sizeof(notification));
	notification.add_header = AddHeader;
	notification.get_header = GetHeader;
	notification.set_header = SetHeader;
}
#endif
#endif
