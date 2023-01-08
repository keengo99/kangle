#ifndef KCONTENTTRANSFER_H
#define KCONTENTTRANSFER_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

/*
ÄÚÈÝ±ä»»
*/
class KContentTransfer : public KHttpStream
{
public:
	KContentTransfer(KWriteStream *st, bool autoDelete) : KHttpStream(st, autoDelete) 
	{

	}
	bool support_sendfile(void* arg) override {
		return false;
	}
	StreamState write_all(void *rq,const char *str,int len)override;
};
#endif
