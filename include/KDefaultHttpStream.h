#ifndef KDEFAULTHTTPSTREAM_H
#define KDEFAULTHTTPSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

class KDefaultHttpStream : public KWriteStream {
public:
protected:
	int write(void *rq, const char *buf, int len) override
	{
		return ((KHttpRequest *)rq)->Write(buf, len);
	}
};
#endif
