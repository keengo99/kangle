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
		this->before_cache = need_check;
	}
	KDsoAsyncFetchObject(void *model_ctx,bool need_check)
	{
		memset(&ctx, 0, sizeof(ctx));
		ctx.module = model_ctx;
		this->before_cache = need_check;
	}
	inline kgl_upstream *GetAsyncUpstream()
	{
		KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
		return (kgl_upstream *)dr->us;
	}
	void on_readhup(KHttpRequest* rq) override;
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
	kgl_async_context ctx;
private:
	void init();
};
kgl_async_context * kgl_get_async_context(KCONN cn);
#endif

