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
	for (;;) {
		ZSTD_outBuffer out;
		out.dst = (void*)buffer;
		out.pos = 0;
		out.size = sizeof(buffer);
		size_t result = ZSTD_compressStream2(ctx->ctx, &out, &in, op);
		if (ZSTD_isError(result)) {
			return KGL_EDATA_FORMAT;
		}
		if (out.pos > 0) {
			KGL_RESULT ret = ctx->down_body.f->write(ctx->down_body.ctx, buffer, (int)out.pos);
			if (ret != KGL_OK) {
				return ret;
			}
		}
		if (op == ZSTD_e_continue) {
			if (in.pos == in.size) {
				break;
			}
		} else if (result == 0) {
			break;
		}
	}
	return KGL_OK;
}
static KGL_RESULT zstd_write(kgl_response_body_ctx* rq, const char* str, int len) {
	if (len < 0 || (len > 0 && str == NULL)) {
		return KGL_EINVALID_PARAMETER;
	}
	return zstd_compress((zstd_context*)rq, (const uint8_t*)str, (size_t)len, ZSTD_e_continue);
}
static KGL_RESULT zstd_flush(kgl_response_body_ctx* rq) {
	KGL_RESULT result2 = zstd_compress((zstd_context*)rq, NULL, 0, ZSTD_e_flush);
	if (result2 != KGL_OK) {
		return result2;
	}
	return forward_flush(rq);
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
bool pipe_zstd_compress(int level, kgl_response_body* body, int64_t content_len) {
	auto ctx = ZSTD_createCCtx();
	if (ctx == NULL) {
		return false;
	}
	if (ZSTD_isError(ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level))) {
		ZSTD_freeCCtx(ctx);
		return false;
	}
	/*
	 * A streaming context without a size uses the full window of the level
	 * (2MB at level 5) for every response. A window that covers the whole
	 * body gives the same output with far less memory. 512KB is not above
	 * the default window of any level, so larger bodies keep the default.
	 * The size is only a hint: a body of another length still compresses.
	 */
	if (content_len > 0 && content_len < (1 << 19)) {
		int window_log = 1;
		while (((int64_t)1 << window_log) < content_len) {
			window_log++;
		}
		ZSTD_bounds bounds = ZSTD_cParam_getBounds(ZSTD_c_windowLog);
		if (!ZSTD_isError(bounds.error)) {
			if (window_log < bounds.lowerBound) {
				window_log = bounds.lowerBound;
			}
			ZSTD_CCtx_setParameter(ctx, ZSTD_c_windowLog, window_log);
		}
	}
	zstd_context* zstd = new zstd_context;
	zstd->ctx = ctx;
	pipe_response_body(zstd, &zstd_function, body);
	return true;
}
#endif
