#include "filter.h"

/**
������������Сд��e����*
*/
static KGL_RESULT write_all(kgl_response_body_ctx*model_ctx, const char *buf, int size)
{
	filter_context* ctx = (filter_context*)model_ctx;
	KGL_RESULT result = KGL_OK;
	while (size>0) {
		if (*buf == 'e') {
			result = ctx->down.f->write(ctx->down.ctx, "*", 1);
		} else {
			result = ctx->down.f->write(ctx->down.ctx, buf, 1);
		}
		if (result != KGL_OK) {
			return result;
		}
		size--;
		buf++;
	}
	return KGL_OK;
}
static KGL_RESULT unsupport_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc) {
	for (int i = 0; i < bc; i++) {
		KGL_RESULT result = write_all(ctx, (char *)bufs[i].iov_base, bufs[i].iov_len);
		if (result != KGL_OK) {
			return result;
		}
	}
	return KGL_OK;
}
static KGL_RESULT flush(kgl_response_body_ctx*model_ctx)
{
	filter_context* ctx = (filter_context*)model_ctx;
	return ctx->down.f->flush(ctx->down.ctx);
}
static KGL_RESULT release(kgl_response_body_ctx* model_ctx, KGL_RESULT result)
{
	filter_context *ctx = (filter_context *)model_ctx;
	result = ctx->down.f->close(ctx->down.ctx, result);
	delete ctx;
	return result;
}
static bool support_sendfile(kgl_response_body_ctx* ctx) {
	return false;
}
static kgl_response_body_function filter_body_function = {
	unsupport_writev,
	write_all,
	flush,
	support_sendfile,
	NULL,
	release,
};
static KGL_RESULT filter_tee_body( kgl_response_body_ctx* ctx, KREQUEST rq, kgl_response_body* body) {
	filter_context* filter_ctx = (filter_context*)ctx;
	if (!body) {
		delete filter_ctx;
		return KGL_OK;
	}
	filter_ctx->down = *body;
	body->ctx = ctx;
	body->f = &filter_body_function;
	return KGL_OK;
}
static kgl_out_filter filter = {
	sizeof(kgl_out_filter),
	KGL_FILTER_NOT_CACHE| KGL_FILTER_CACHE|KGL_FILTER_NOT_CHANGE_LENGTH,
	filter_tee_body,
};
void register_filter(KREQUEST r, kgl_access_context *ctx, filter_context *model_ctx)
{
	ctx->f->support_function(r, ctx->cn, KF_REQ_OUT_FILTER, &filter, (void **)&model_ctx);
}
