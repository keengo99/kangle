#include "KContentTransfer.h"
#include "KChunked.h"
#include "kmalloc.h"
#include "KGzip.h"
#include "KFilterContext.h"

StreamState KContentTransfer::write_all(KHttpRequest *rq, const char *str,int len)
{
	assert(rq->of_ctx);
	if(rq->of_ctx->checkFilter(rq,str,len)==JUMP_DENY){
		KBIT_SET(rq->filter_flags,RQ_RESPONSE_DENY);
		KBIT_SET(rq->flags,RQ_CONNECTION_CLOSE);
		return STREAM_WRITE_FAILED;
	}
	return KHttpStream::write_all(rq, str,len);
}
