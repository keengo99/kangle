#include "KBigObjectStream.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
StreamState KBigObjectStream::write_all(KHttpRequest *rq, const char *buf, int len)
{
	assert(rq->bo_ctx);
	KGL_RESULT result = rq->bo_ctx->StreamWrite(rq->range_from, buf, len);
	rq->range_from += len;
	return result;
#if 0
	const char *hot = buf;
	if (rq->bo_ctx) {
		rq->bo_ctx->stream_write(buf, len);
		if (rq->bo_ctx->offset_for_align > 0) {
			int skip = MIN(len, rq->bo_ctx->offset_for_align);
			rq->bo_ctx->offset_for_align -= skip;
			len -= skip;
			hot += len;
		}
		if (rq->bo_ctx->offset_for_align > 0) {
			return STREAM_WRITE_SUCCESS;
		}
		len = (int)MIN((INT64)len, rq->bo_ctx->left_read);
		//printf("len=[%d] left_read=[%d]\n", len, rq->bo_ctx->left_read);
		rq->bo_ctx->range_from += len;
		rq->bo_ctx->left_read -= len;
	}
	if (len == 0) {
		return STREAM_WRITE_SUCCESS;
	}	
	return rq->write_all(hot, len);
#endif
}
StreamState KBigObjectStream::write_end(KHttpRequest *rq,KGL_RESULT result)
{
	return result;
}
#endif
//}}

