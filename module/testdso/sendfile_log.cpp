#include "filter.h"
#include "katom.h"
sendfile_log_context sendfile_context = { 0 };
static KGL_RESULT write_all(KREQUEST rq, kgl_filter_context* ctx, const char* buf, int size) {
	return ctx->f->write_all(rq, ctx->cn, buf, size);
}
static KGL_RESULT flush(KREQUEST rq, kgl_filter_context* ctx) {
	return ctx->f->flush(rq, ctx->cn);
}
static KGL_RESULT write_end(KREQUEST rq, kgl_filter_context* ctx, KGL_RESULT result) {
	return ctx->f->write_end(rq, ctx->cn, result);
}
static KGL_RESULT sendfile(KREQUEST rq, kgl_filter_context* ctx, KASYNC_FILE fp, int64_t* length) {
	sendfile_log_context* log_ctx = (sendfile_log_context*)ctx->module;
	katom_inc((void*)&log_ctx->total_count);
	katom_add64((void*)&log_ctx->total_length, *length);
	return ctx->f->sendfile(rq, ctx->cn, fp, length);
}
static void release(void* model_ctx) {
	
}
static kgl_filter sendfile_filter = {
	"sendfile_log",
	KGL_FILTER_CACHE | KGL_FILTER_NOT_CACHE | KGL_FILTER_NOT_CHANGE_LENGTH,
	write_all,
	flush,
	sendfile,
	write_end,
	release
};
void register_sendfile_log(KREQUEST r, kgl_access_context* ctx, sendfile_log_context* model_ctx) {
	ctx->f->support_function(r, ctx->cn, KF_REQ_FILTER, &sendfile_filter, (void**)&model_ctx);
}
