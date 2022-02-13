#include "KBrotli.h"
#include "KConfig.h"
#ifdef ENABLE_BROTLI
KBrotliCompress::KBrotliCompress()
{
	state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, conf.br_level);
}
KBrotliCompress::~KBrotliCompress()
{
	BrotliEncoderDestroyInstance(state);
}
KGL_RESULT KBrotliCompress::Compress(KHttpRequest *rq, const uint8_t **str, size_t len, BrotliEncoderOperation op)
{
	char out[8192];
	do {
		size_t available_out = sizeof(out);
		uint8_t *next_out = (uint8_t *)out;
		if (!BrotliEncoderCompressStream(state, op, &len, str, &available_out, &next_out, NULL)) {
			return KGL_EDATA_FORMAT;
		}
		int out_len = sizeof(out) - available_out;
		if (out_len > 0) {
			KGL_RESULT ret = st->write_all(rq, out, out_len);
			if (ret != KGL_OK) {
				return ret;
			}
		}
	} while (BrotliEncoderHasMoreOutput(state));
	return KGL_OK;
}
KGL_RESULT KBrotliCompress::write_all(KHttpRequest *rq, const char *str, int len)
{
	return Compress(rq, (const uint8_t **)&str, (size_t)len, BROTLI_OPERATION_PROCESS);
}
KGL_RESULT KBrotliCompress::flush(KHttpRequest *rq)
{
	KGL_RESULT result2 = Compress(rq,NULL,0, BROTLI_OPERATION_FLUSH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	return KGL_OK;
}
KGL_RESULT KBrotliCompress::write_end(KHttpRequest *rq, KGL_RESULT result)
{
	KGL_RESULT result2 = Compress(rq, NULL, 0, BROTLI_OPERATION_FINISH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	return KHttpStream::write_end(rq, result);
}
#endif
