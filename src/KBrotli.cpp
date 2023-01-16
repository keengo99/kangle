#include "KBrotli.h"
#include "KConfig.h"
#include "KPushGate.h"
#ifdef ENABLE_BROTLI
struct brotli_context : kgl_forward_body
{
	BrotliEncoderState* state;
};
KBrotliCompress::KBrotliCompress() : KCompressStream(NULL)
{
	state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, conf.br_level);
}
KBrotliCompress::~KBrotliCompress()
{
	BrotliEncoderDestroyInstance(state);
}
KGL_RESULT KBrotliCompress::Compress(void *rq, const uint8_t **str, size_t len, BrotliEncoderOperation op)
{
	char out[8192];
	do {
		size_t available_out = sizeof(out);
		uint8_t *next_out = (uint8_t *)out;
		if (!BrotliEncoderCompressStream(state, op, &len, str, &available_out, &next_out, NULL)) {
			return KGL_EDATA_FORMAT;
		}
		int out_len = (int)(sizeof(out) - available_out);
		if (out_len > 0) {
			KGL_RESULT ret = st->write_all(rq, out, out_len);
			if (ret != KGL_OK) {
				return ret;
			}
		}
	} while (BrotliEncoderHasMoreOutput(state));
	return KGL_OK;
}
KGL_RESULT KBrotliCompress::write_all(void*rq, const char *str, int len)
{
	return Compress(rq, (const uint8_t **)&str, (size_t)len, BROTLI_OPERATION_PROCESS);
}
KGL_RESULT KBrotliCompress::flush(void *rq)
{
	KGL_RESULT result2 = Compress(rq,NULL,0, BROTLI_OPERATION_FLUSH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	return KGL_OK;
}
KGL_RESULT KBrotliCompress::write_end(void *rq, KGL_RESULT result)
{
	KGL_RESULT result2 = Compress(rq, NULL, 0, BROTLI_OPERATION_FINISH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	return KHttpStream::write_end(rq, result);
}
KGL_RESULT brotli_compress(brotli_context *ctx, const uint8_t** str, size_t len, BrotliEncoderOperation op)
{
	char out[8192];
	do {
		size_t available_out = sizeof(out);
		uint8_t* next_out = (uint8_t*)out;
		if (!BrotliEncoderCompressStream(ctx->state, op, &len, str, &available_out, &next_out, NULL)) {
			return KGL_EDATA_FORMAT;
		}
		int out_len = (int)(sizeof(out) - available_out);
		if (out_len > 0) {
			KGL_RESULT ret = ctx->down_body.f->write(ctx->down_body.ctx, out, out_len);
			if (ret != KGL_OK) {
				return ret;
			}
		}
	} while (BrotliEncoderHasMoreOutput(ctx->state));
	return KGL_OK;
}
static KGL_RESULT brotli_write(kgl_response_body_ctx * rq, const char* str, int len) 
{
	return brotli_compress((brotli_context*)rq, (const uint8_t**)&str, (size_t)len, BROTLI_OPERATION_PROCESS);
}
static KGL_RESULT brotli_flush(kgl_response_body_ctx* rq) {
	KGL_RESULT result2 = brotli_compress((brotli_context*)rq, NULL, 0, BROTLI_OPERATION_FLUSH);
	if (result2 != KGL_OK) {
		return forward_flush((kgl_response_body_ctx*)rq);
	}
	return KGL_OK;
}
static KGL_RESULT brotli_close(kgl_response_body_ctx* rq,KGL_RESULT result) {
	brotli_context* ctx = (brotli_context*)rq;
	KGL_RESULT result2 = brotli_compress(ctx, NULL, 0, BROTLI_OPERATION_FINISH);
	if (result2 != KGL_OK) {
		result = forward_close(rq, result2);
	} else {
		result = forward_close(rq, result);
	}
	delete ctx;
	return result;
}
static kgl_response_body_function brotli_function = {
	unsupport_writev<brotli_write>,
	brotli_write,
	brotli_flush,
	support_sendfile_false,
	unsupport_sendfile,
	brotli_close,
};
bool pipe_brotli_compress(int level, kgl_response_body* body) {
	BrotliEncoderState *state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	if (!state) {
		return false;
	}
	BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, level);
	brotli_context* br = new brotli_context;
	br->state = state;
	pipe_response_body(br, &brotli_function, body);
	return true;
}
#endif
