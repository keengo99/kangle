#include "KHttpFilterHook.h"
#include "KHttpRequest.h"
#include "KHttpFilterDso.h"
#include "KHttpFilterContext.h"
#include "KHttpStream.h"
#include "KHttpFilterStream.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0
void KHttpFilterHookCollect::add_hook(KHttpFilterDso *fd)
{
	KHttpFilterHook *hook = new KHttpFilterHook(fd);
	hook->next = NULL;
	if (last) {
		last->next = hook;
	} else {
		head = hook;
	}
	last = hook;
}
KHttpFilterHookCollect::KHttpFilterHookCollect()
{
	head = NULL;
	last = NULL;
}
KHttpFilterHookCollect::~KHttpFilterHookCollect()
{
	while (head) {
		last = head->next;
		delete head;
		head = last;
	}
}
DWORD KHttpFilterHookCollect::process(KHttpRequest *rq,DWORD notificationType, LPVOID pvNotification)
{
	KHttpFilterHook *hook = head;
	while (hook) {
		rq->init_http_filter();
		DWORD disabled_flags = rq->http_filter_ctx->restore(hook->dso->index);
		if (KBIT_TEST(disabled_flags,notificationType)) {
			//disabled
			hook = hook->next;
			continue;
		}
		DWORD ret = hook->dso->kgl_filter_process(
				&rq->http_filter_ctx->ctx,
				notificationType,
				pvNotification);
		rq->http_filter_ctx->save(hook->dso->index);
		if (ret==KF_STATUS_REQ_NEXT_NOTIFICATION) {
			hook = hook->next;
			continue;
		}
		if (ret==KF_STATUS_REQ_FINISHED) {
			KBIT_SET(rq->flags,RQ_CONNECTION_CLOSE);
		}
		return ret;
	}
	return KF_STATUS_REQ_HANDLED_NOTIFICATION;
}
void KHttpFilterHookCollect::buildRawStream(DWORD type,KHttpRequest *rq,KHttpStream **raw_head,KHttpStream **raw_end)
{
	KHttpFilterHook *hook = head;
	while (hook) {
		rq->init_http_filter();
		DWORD disabled_flags = rq->http_filter_ctx->restore(hook->dso->index);
		if (KBIT_TEST(disabled_flags,type)) {
			//disabled
			hook = hook->next;
			continue;
		}
		KHttpStream *st = new KHttpFilterStream(rq,hook,type);
		if (*raw_end==NULL) {
			*raw_head = st;
		} else {
			(*raw_end)->connect(st,true);
		}
		*raw_end = st;
		hook = hook->next;
	}
}
int KHttpFilterHookCollectUrlMap::check_urlmap(KHttpRequest *rq)
{
	if (rq->file==NULL) {
		return JUMP_ALLOW;
	}
	kgl_filter_url_map notification;
	notification.pszURL = rq->url->getUrl();
	notification.pszPhysicalPath = (char *)rq->file->getName();
	notification.cbPathBuff = rq->file->getNameLen();
	DWORD ret = process(rq,KF_NOTIFY_URL_MAP,&notification);
	xfree((void *)notification.pszURL);
	switch (ret) {
	case KF_STATUS_REQ_HANDLED_NOTIFICATION:
	case KF_STATUS_REQ_NEXT_NOTIFICATION:
	case KF_STATUS_REQ_READ_NEXT:
		return JUMP_ALLOW;
	default:
		return JUMP_DENY;
	}
}
#endif
#endif
