#ifndef KCOMPRESS_STREAM_H_99
#define KCOMPRESS_STREAM_H_99
#include "KHttpStream.h"
class KHttpObject;
class KCompressStream : public KHttpStream
{
public:
};
KCompressStream *create_compress_stream(KHttpRequest *rq, KHttpObject *obj, int64_t content_len);
#endif

