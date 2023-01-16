#include "KContentTransfer.h"
#include "KChunked.h"
#include "kmalloc.h"
#include "KGzip.h"
#include "KFilterContext.h"

StreamState KContentTransfer::write_all(void *arg, const char *str,int len)
{
	KHttpRequest* rq = (KHttpRequest*)arg;
	assert(rq->of_ctx);
	if(rq->of_ctx->checkFilter(rq,str,len)==JUMP_DENY){
		KBIT_SET(rq->ctx.filter_flags,RQ_RESPONSE_DENY);
		KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
		return STREAM_WRITE_FAILED;
	}
	return KHttpStream::write_all(rq, str,len);
}
