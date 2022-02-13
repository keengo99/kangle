#ifndef KBIGOBJECTSTREAM_H
#define KBIGOBJECTSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"
#include "KBigObjectContext.h"
#include "KHttpObject.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
class KBigObjectStream : public KWriteStream
{
public:
	KBigObjectStream(KHttpRequest *rq)
	{
		assert(rq->bo_ctx);
		sbo = rq->bo_ctx->getSharedBigObject();
	}
	~KBigObjectStream()
	{

	}
	StreamState write_all(KHttpRequest *rq, const char *buf, int len);
	StreamState write_end(KHttpRequest *rq,KGL_RESULT result);
private:
	KSharedBigObject *sbo;
};
#endif
//}}
#endif
