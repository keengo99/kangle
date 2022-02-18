#include "filter.h"
/**
测试用例，把小写的e换成*
*/
static KGL_RESULT write_all(KREQUEST rq, kgl_filter_context *ctx, const char *buf, DWORD size)
{
	KGL_RESULT result = KGL_OK;
	while (size>0) {
		if (*buf == 'e') {
			result = ctx->f->write_all(rq, ctx->cn, "*", 1);
		} else {
			result = ctx->f->write_all(rq, ctx->cn, buf, 1);
		}
		if (result != KGL_OK) {
			return result;
		}
		size--;
		buf++;
	}
	return KGL_OK;
}
static KGL_RESULT flush(KREQUEST rq, kgl_filter_context *ctx)
{
	return ctx->f->flush(rq, ctx->cn);
}
static KGL_RESULT write_end(KREQUEST rq, kgl_filter_context *ctx, KGL_RESULT result)
{
	return ctx->f->write_end(rq, ctx->cn, result);
}
static void release(void *model_ctx)
{
	filter_context *ctx = (filter_context *)model_ctx;
	delete ctx;
}
static kgl_filter filter = {
	"test",
	write_all,
	flush,
	write_end,
	release
};
void register_filter(KREQUEST r, kgl_access_context *ctx, filter_context *model_ctx)
{
	ctx->f->support_function(r, ctx->cn, KF_REQ_FILTER, &filter, (void **)&model_ctx);
}
