#ifndef KBROTLI_H_99
#define KBROTLI_H_99
#include "KCompressStream.h"
#include "global.h"
#ifdef ENABLE_BROTLI
#include "brotli/encode.h"

class KBrotliCompress : public KCompressStream {
public:
	KBrotliCompress();
	~KBrotliCompress();
	KGL_RESULT flush(KHttpRequest *rq);
	KGL_RESULT write_all(KHttpRequest *rq, const char *str, int len);
	KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result);
private:
	KGL_RESULT Compress(KHttpRequest *rq, const uint8_t **str, size_t len, BrotliEncoderOperation op);
	BrotliEncoderState *state;
};
#endif
#endif
