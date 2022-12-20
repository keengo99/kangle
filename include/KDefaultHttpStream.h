#ifndef KDEFAULTHTTPSTREAM_H
#define KDEFAULTHTTPSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

class KDefaultHttpStream : public KWriteStream {
public:
	KGL_RESULT write_end(void* arg, KGL_RESULT result) override
	{
		KHttpRequest* rq = ((KHttpRequest*)arg);		
		if (result != KGL_OK) {
			KHttpObject* obj = rq->ctx->obj;
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE | RQ_BODY_NOT_COMPLETE);
			//如果是http2和http3的情况下，此处要向下游传递错误，调用terminal stream
			//避免下游会误缓存
		}
		return result;
	}
protected:
	int write(void *rq, const char *buf, int len) override
	{
		return ((KHttpRequest *)rq)->Write(buf, len);
	}

};
#endif
