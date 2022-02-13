#ifndef KDEFAULTHTTPSTREAM_H
#define KDEFAULTHTTPSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

class KDefaultHttpStream : public KWriteStream {
public:
protected:
	int write(KHttpRequest *rq, const char *buf, int len)
	{
		return rq->Write(buf, len);
	}
};
#endif
