#include "filter.h"

static KGL_RESULT write_all(kgl_response_body_ctx*model_ctx, const char *buf, int size)
{
	kgl_filter_footer* ctx = (kgl_filter_footer*)model_ctx;
	
	if (ctx->footer.replace) {
		return KGL_OK;
	}
	if (ctx->footer.head && !ctx->footer.added && ctx->footer.data) {
		KGL_RESULT result;
		ctx->footer.added = true;
		if (ctx->footer.data->len > 0) {
			result = ctx->down.f->write(ctx->down.ctx, (char *)ctx->footer.data->data, ctx->footer.data->len);
			if (result != KGL_OK) {
				return result;
			}
		}
	}
	return ctx->down.f->write(ctx->down.ctx, buf, size);
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
	kgl_filter_footer* ctx = (kgl_filter_footer*)model_ctx;
	return ctx->down.f->flush(ctx->down.ctx);
}
static KGL_RESULT filter_close(kgl_response_body_ctx* model_ctx, KGL_RESULT result)
{
	kgl_filter_footer*ctx = (kgl_filter_footer*)model_ctx;
	if (result >=KGL_OK && (!ctx->footer.head || ctx->footer.replace)) {
		if (ctx->footer.data->len > 0) {
			result = ctx->down.f->write(ctx->down.ctx, ctx->footer.data->data, ctx->footer.data->len);
		}
	}
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
	filter_close,
};
static KGL_RESULT filter_tee_body( kgl_response_body_ctx* ctx, KREQUEST rq, kgl_response_body* body) {
	kgl_filter_footer* filter_ctx = (kgl_filter_footer*)ctx;
	if (!body) {
		delete filter_ctx;
		return KGL_OK;
	}
	filter_ctx->down = *body;
	body->ctx = ctx;
	body->f = &filter_body_function;
	return KGL_OK;
}
static kgl_out_filter footer_filter = {
	sizeof(kgl_out_filter),
	KGL_FILTER_NOT_CACHE,
	filter_tee_body,
};
void register_footer(KREQUEST r, kgl_access_context *ctx, kgl_filter_footer*model_ctx)
{
	ctx->f->support_function(r, ctx->cn, KF_REQ_OUT_FILTER, &footer_filter, (void **)&model_ctx);
}
