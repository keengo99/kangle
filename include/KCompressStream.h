#ifndef KCOMPRESS_STREAM_H_99
#define KCOMPRESS_STREAM_H_99
#include "KHttpStream.h"

class KHttpObject;
class KHttpRequest;
#if 0
class KCompressStream : public KHttpStream
{
public:
	KCompressStream(KWriteStream* st) : KHttpStream(st) {

	}
};
#endif
bool pipe_compress_stream(KHttpRequest* rq, KHttpObject* obj, int64_t content_len, kgl_response_body* body);
#endif

