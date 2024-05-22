#include "KZstd.h"
#include "KConfig.h"
#include "KPushGate.h"
#ifdef ENABLE_ZSTD
struct zstd_context : kgl_forward_body
{
	ZSTD_CCtx* ctx;
};
static KGL_RESULT zstd_compress(zstd_context* ctx, const uint8_t* str, size_t len, ZSTD_EndDirective op) {
	char buffer[8192];
	ZSTD_inBuffer in;
	in.src = str;
	in.size = len;
	in.pos = 0;
	do {
		size_t result;
		do {
			ZSTD_outBuffer out;
			out.dst = (void*)buffer;
			out.pos = 0;
			out.size = sizeof(buffer);
			result = ZSTD_compressStream2(ctx->ctx, &out, &in, op);
			if (out.pos > 0) {
				KGL_RESULT ret = ctx->down_body.f->write(ctx->down_body.ctx, buffer, (int)out.pos);
				if (ret != KGL_OK) {
					return ret;
				}
			}
		} while (result > 0);
	} while (in.pos!=in.size);
	return KGL_OK;
}
static KGL_RESULT zstd_write(kgl_response_body_ctx* rq, const char* str, int len) {
	return zstd_compress((zstd_context*)rq, (const uint8_t*)str, (size_t)len, ZSTD_e_continue);
}
static KGL_RESULT zstd_flush(kgl_response_body_ctx* rq) {
	KGL_RESULT result2 = zstd_compress((zstd_context*)rq, NULL, 0, ZSTD_e_flush);
	if (result2 != KGL_OK) {
		return forward_flush((kgl_response_body_ctx*)rq);
	}
	return KGL_OK;
}
static KGL_RESULT zstd_close(kgl_response_body_ctx* rq, KGL_RESULT result) {
	zstd_context* ctx = (zstd_context*)rq;
	KGL_RESULT result2 = zstd_compress(ctx, NULL, 0, ZSTD_e_end);
	if (result2 != KGL_OK) {
		result = forward_close(rq, result2);
	} else {
		result = forward_close(rq, result);
	}	
	ZSTD_freeCCtx(ctx->ctx);
	delete ctx;
	return result;
}
static kgl_response_body_function zstd_function = {
	unsupport_writev<zstd_write>,
	zstd_write,
	zstd_flush,
	support_sendfile_false,
	unsupport_sendfile,
	zstd_close,
};
bool pipe_zstd_compress(int level, kgl_response_body* body) {
	auto ctx = ZSTD_createCCtx();
	if (ctx == NULL) {
		return false;
	}
	ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
	zstd_context* zstd = new zstd_context;
	zstd->ctx = ctx;
	pipe_response_body(zstd, &zstd_function, body);
	return true;
}
#endif
