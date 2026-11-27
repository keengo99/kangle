#include "KBrotli.h"
#include "KConfig.h"
#include "KPushGate.h"
#ifdef ENABLE_BROTLI
struct brotli_context : kgl_forward_body
{
	BrotliEncoderState* state;
};
KGL_RESULT brotli_compress(brotli_context *ctx, const uint8_t** str, size_t len, BrotliEncoderOperation op)
{
	char out[8192];
	const uint8_t* empty = NULL;
	if (str == NULL) {
		str = &empty;
	}
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
	} while (len > 0 || BrotliEncoderHasMoreOutput(ctx->state));
	return KGL_OK;
}
static KGL_RESULT brotli_write(kgl_response_body_ctx * rq, const char* str, int len) 
{
	if (len < 0 || (len > 0 && str == NULL)) {
		return KGL_EINVALID_PARAMETER;
	}
	return brotli_compress((brotli_context*)rq, (const uint8_t**)&str, (size_t)len, BROTLI_OPERATION_PROCESS);
}
static KGL_RESULT brotli_flush(kgl_response_body_ctx* rq) {
	KGL_RESULT result2 = brotli_compress((brotli_context*)rq, NULL, 0, BROTLI_OPERATION_FLUSH);
	if (result2 != KGL_OK) {
		return result2;
	}
	return forward_flush(rq);
}
static KGL_RESULT brotli_close(kgl_response_body_ctx* rq,KGL_RESULT result) {
	brotli_context* ctx = (brotli_context*)rq;
	KGL_RESULT result2 = brotli_compress(ctx, NULL, 0, BROTLI_OPERATION_FINISH);
	if (result2 != KGL_OK) {
		result = forward_close(rq, result2);
	} else {
		result = forward_close(rq, result);
	}
	BrotliEncoderDestroyInstance(ctx->state);
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
bool pipe_brotli_compress(int level, kgl_response_body* body, int64_t content_len) {
	BrotliEncoderState *state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
	if (!state) {
		return false;
	}
	if (!BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, level)) {
		BrotliEncoderDestroyInstance(state);
		return false;
	}
	/*
	 * Streaming without a size uses the default 4MB window and its hash
	 * tables. A window that covers the whole body gives the same output with
	 * less memory. The size is a hint only, so a different body length is fine.
	 */
	if (content_len > 0 && content_len <= (int64_t)UINT32_MAX) {
		BrotliEncoderSetParameter(state, BROTLI_PARAM_SIZE_HINT, (uint32_t)content_len);
		int lgwin = BROTLI_MIN_WINDOW_BITS;
		/* the usable window is (1 << lgwin) - 16 bytes. */
		while (lgwin < BROTLI_DEFAULT_WINDOW && ((int64_t)1 << lgwin) - 16 < content_len) {
			lgwin++;
		}
		if (lgwin < BROTLI_DEFAULT_WINDOW) {
			BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, (uint32_t)lgwin);
		}
	}
	brotli_context* br = new brotli_context;
	br->state = state;
	pipe_response_body(br, &brotli_function, body);
	return true;
}
#endif
