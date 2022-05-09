#include "KBigObjectStream.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
StreamState KBigObjectStream::write_all(void*arg, const char *buf, int len)
{
	auto rq = (KHttpRequest*)arg;
	assert(rq->bo_ctx);
	KGL_RESULT result = rq->bo_ctx->StreamWrite(rq->sink->data.range_from, buf, len);
	rq->sink->data.range_from += len;
	return result;
}
StreamState KBigObjectStream::write_end(void*rq,KGL_RESULT result)
{
	return result;
}
#endif
//}}

