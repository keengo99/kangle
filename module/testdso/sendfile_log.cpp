#include "filter.h"
#include "katom.h"
volatile int64_t sendfile_total_length = 0;
volatile int32_t sendfile_total_count = 0;
KGL_RESULT sendfile_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc) {
	sendfile_log_context* send_ctx = (sendfile_log_context*)ctx;
	return send_ctx->down.f->writev(send_ctx->down.ctx, bufs, bc);
}
static KGL_RESULT write_all(kgl_response_body_ctx *ctx, const char* buf, int size) {
	sendfile_log_context* send_ctx = (sendfile_log_context*)ctx;
	return send_ctx->down.f->write(send_ctx->down.ctx, buf, size);
}
static KGL_RESULT flush(kgl_response_body_ctx * ctx) {
	sendfile_log_context* send_ctx = (sendfile_log_context*)ctx;
	return send_ctx->down.f->flush(send_ctx->down.ctx);
}
static KGL_RESULT sendfile_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	sendfile_log_context* log_ctx = (sendfile_log_context*)ctx;
	result = log_ctx->down.f->close(log_ctx->down.ctx, result);
	delete log_ctx;
	return result;
}
bool support_sendfile(kgl_response_body_ctx* ctx) {
	sendfile_log_context* log_ctx = (sendfile_log_context*)ctx;
	return log_ctx->down.f->support_sendfile(log_ctx->down.ctx);
}
static KGL_RESULT sendfile(kgl_response_body_ctx* ctx, KASYNC_FILE fp, int64_t* length) {
	sendfile_log_context* log_ctx = (sendfile_log_context*)ctx;
	katom_inc((void*)&sendfile_total_count);
	katom_add64((void*)&sendfile_total_length, *length);
	return log_ctx->down.f->sendfile(log_ctx->down.ctx, fp, length);
}
static kgl_response_body_function sendfile_body_function = {
	sendfile_writev,
	write_all,
	flush,
	support_sendfile,
	sendfile,
	sendfile_close,
};
static KGL_RESULT tee_body( kgl_response_body_ctx *ctx, KREQUEST rq, kgl_response_body* body) {
	sendfile_log_context* send_ctx = (sendfile_log_context*)ctx;
	if (!body) {
		delete send_ctx;
		return KGL_OK;
	}
	send_ctx->down = *body;
	body->ctx = ctx;
	body->f = &sendfile_body_function;
	return KGL_OK;
}

static kgl_filter sendfile_filter = {
	sizeof(kgl_filter),
	KGL_FILTER_CACHE  | KGL_FILTER_NOT_CHANGE_LENGTH|KGL_FILTER_NOT_CACHE,
	tee_body
};
void register_sendfile_log(KREQUEST r, kgl_access_context* ctx, sendfile_log_context* model_ctx) {
	ctx->f->support_function(r, ctx->cn, KF_REQ_FILTER, &sendfile_filter, (void**)&model_ctx);
}
