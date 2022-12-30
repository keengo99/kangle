#ifndef KDSOASYNCFETCHOBJECT_H
#define KDSOASYNCFETCHOBJECT_H
#include "KFetchObject.h"
#include "KHttpResponseParser.h"
#include "KDsoRedirect.h"

class KDsoAsyncFetchObject : public KFetchObject
{
public:
	KDsoAsyncFetchObject(bool need_check)
	{
		memset(&ctx, 0, sizeof(ctx));
		this->need_check = need_check;
	}
	KDsoAsyncFetchObject(void *model_ctx,bool need_check)
	{
		memset(&ctx, 0, sizeof(ctx));
		ctx.module = model_ctx;
		this->need_check = need_check;
	}
	kgl_upstream *GetAsyncUpstream()
	{
		KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
		return (kgl_upstream *)dr->us;
	}
	bool NeedTempFile(bool upload, KHttpRequest *rq) override
	{
		return false;
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue(KHttpRequest *rq) override
	{
		return false;
	}
#endif
	~KDsoAsyncFetchObject();
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
	uint32_t Check(KHttpRequest* rq) override;
	kgl_async_context ctx;
private:
	void init();
};
kgl_async_context * kgl_get_async_context(KCONN cn);
#endif

