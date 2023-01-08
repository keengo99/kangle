#ifndef KBIGOBJECTSTREAM_H
#define KBIGOBJECTSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"
#include "KBigObjectContext.h"
#include "KHttpObject.h"

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
	StreamState write_all(void*rq, const char *buf, int len) override;
	StreamState write_end(void*rq,KGL_RESULT result) override;
private:
	KSharedBigObject *sbo;
};
#endif
#endif
