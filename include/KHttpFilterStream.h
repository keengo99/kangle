#ifndef KHTTPFILTERSTREAM_H
#define KHTTPFILTERSTREAM_H
#include "KHttpStream.h"
#include "ksapi.h"
class KHttpFilterHook;
class KHttpRequest;
#ifdef ENABLE_KSAPI_FILTER
#if 0
class KHttpFilterStream : public KHttpStream
{
public:
	KHttpFilterStream(KHttpRequest *rq, KHttpFilterHook *hook, DWORD notificationType) 
		: KHttpStream(NULL)
	{
		this->rq = rq;
		this->hook = hook;
		this->notificationType = notificationType;
	}
	StreamState write_all(const char *buf, int len);
	StreamState write_end();
private:
	KHttpRequest *rq;
	KHttpFilterHook *hook;
	DWORD notificationType;
};
#endif
#endif
#endif
