#include <stdio.h>
#include "kstring.h"
#include "ksapi.h"
#include "upstream.h"

static void * create_ctx()
{
	return NULL;
}
static void free_ctx(void *ctx)
{
	test_context *t = (test_context *)ctx;
	delete t;
}
static KGL_RESULT open(KREQUEST r, kgl_async_context *ctx)
{
	test_context *t = (test_context *)ctx->module;
	if (t->model == test_upstream) {
		ctx->out->f->write_status(ctx->out, r, 200);
		KGL_RESULT result = ctx->out->f->write_header_finish(ctx->out, r);
		if (result != KGL_OK) {
			return result;
		}
		return ctx->out->f->write_end(ctx->out, r, ctx->out->f->write_body(ctx->out, r, kgl_expand_string("test_upstream_ok")));
	}
	KGL_RESULT result = ctx->f->open_next(r, ctx->cn, ctx->in, ctx->out, NULL);
	ctx->in = NULL;
	ctx->out = NULL;
	return result;
}

static kgl_async_upstream async_upstream = {
	sizeof(kgl_async_upstream),0,
	"async_test",
	create_ctx,
	free_ctx,
	NULL,
	open
};
void register_global_async_upstream(kgl_dso_version *ver)
{
	KGL_REGISTER_ASYNC_UPSTREAM(ver, &async_upstream);
}
void register_async_upstream(KREQUEST r, kgl_access_context *ctx, test_context *model_ctx)
{
	ctx->f->support_function(r, ctx->cn, KF_REQ_UPSTREAM, &async_upstream, (void **)&model_ctx);
}
