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
	KGL_RESULT flush(void*rq) override;
	KGL_RESULT write_all(void*rq, const char *str, int len) override;
	KGL_RESULT write_end(void*rq, KGL_RESULT result) override;
private:
	KGL_RESULT Compress(void*rq, const uint8_t **str, size_t len, BrotliEncoderOperation op);
	BrotliEncoderState *state;
};
#endif
#endif
