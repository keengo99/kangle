#ifndef KDSOASYNCFETCHOBJECT_H
#define KDSOASYNCFETCHOBJECT_H
#include "KFetchObject.h"
#include "KHttpResponseParser.h"
#include "KDsoRedirect.h"

class KDsoAsyncFetchObject : public KRedirectSource
{
public:
	KDsoAsyncFetchObject(void *model_ctx, uint32_t flag) : KRedirectSource(flag)
	{
		memset(&ctx, 0, sizeof(ctx));
		ctx.module = model_ctx;
	}
	inline kgl_upstream *get_upstream()
	{
		return static_cast<KDsoRedirect *>(brd->rd)->us;
	}
	void on_readhup(KHttpRequest* rq) override;
	~KDsoAsyncFetchObject();
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
	kgl_async_context ctx;
private:
	void init();
};
kgl_async_context * kgl_get_async_context(KCONN cn);
#endif

