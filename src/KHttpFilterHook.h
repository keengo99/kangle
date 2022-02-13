#ifndef KHTTPFILTERHOOK_H
#define KHTTPFILTERHOOK_H
#include "global.h"
#include "kforwin32.h"
#include "ksapi.h"
class KHttpFilterDso;
class KHttpRequest;
class KHttpStream;
#ifdef ENABLE_KSAPI_FILTER
#if 0
class KHttpFilterHook
{
public:
	KHttpFilterHook(KHttpFilterDso *dso)
	{
		this->dso = dso;
	}
	KHttpFilterDso *dso;
	KHttpFilterHook *next;
};
class KHttpFilterHookCollect
{
public:
	KHttpFilterHookCollect();
	~KHttpFilterHookCollect();
	void add_hook(KHttpFilterDso *fd);
	DWORD process(KHttpRequest *rq,DWORD notificationType, LPVOID pvNotification);
	void buildRawStream(DWORD type,KHttpRequest *rq,KHttpStream **head,KHttpStream **end);
private:
	KHttpFilterHook *head;
	KHttpFilterHook *last;
};
class KHttpFilterHookCollectUrlMap : public KHttpFilterHookCollect
{
public:
	int check_urlmap(KHttpRequest *rq);
};
class KHttpFilterHookCollectLog : public KHttpFilterHookCollect
{
};
void init_kgl_filter_response(kgl_filter_response &notification);
void init_kgl_filter_request(kgl_filter_request &notification);
#endif
#endif
#endif

