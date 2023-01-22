#include <stdio.h>
#include <sstream>
#include "kstring.h"
#include "ksapi.h"
#include "upstream.h"
#include "filter.h"

static void * create_ctx()
{
	test_context* t = new test_context(test_upstream);
	return t;
}
static void free_ctx(void *ctx)
{
	test_context *t = (test_context *)ctx;
	delete t;
}
static KGL_RESULT open(KREQUEST r, kgl_async_context *ctx)
{
	test_context *t = (test_context *)ctx->module;
	KGL_RESULT result;
	switch (t->model) {
	case test_next:
		return KGL_NEXT;
	case test_websocket:
	{
		ctx->out->f->write_status(ctx->out->ctx, 101);
		ctx->out->f->write_header(ctx->out->ctx, kgl_header_connection, _KS("Upgrade"));
		ctx->out->f->write_header(ctx->out->ctx, kgl_header_upgrade, _KS("test"));
		ctx->out->f->write_header(ctx->out->ctx, kgl_header_cache_control, _KS("no-cache, no-store"));
		kgl_response_body body;
		result = ctx->out->f->write_header_finish(ctx->out->ctx, -1, &body);
		if (result != KGL_OK) {
			return result;
		}
		int left_read = (int)ctx->in->f->body.get_left(ctx->in->body_ctx);
		assert(-1 == left_read);
		for (;;) {
			char buf[8];
			int len = ctx->in->f->body.read(ctx->in->body_ctx, buf, sizeof(buf));
			if (len <= 0) {
				break;
			}
			body.f->write(body.ctx, buf, len);
		}
		return body.f->close(body.ctx, KGL_OK);
	}
	case test_chunk:
	{
		kgl_response_body body;
		ctx->out->f->write_status(ctx->out->ctx,  200);
		result = ctx->out->f->write_header_finish(ctx->out->ctx, -1, &body);
		if (result != KGL_OK) {
			return result;
		}
		body.f->write(body.ctx, _KS("he"));
		return body.f->close(body.ctx, body.f->write(body.ctx, _KS("llo")));
	}
	case test_upstream:
	{
		kgl_response_body body;
		int64_t content_length = sizeof("test_upstream_ok")-1;
		ctx->out->f->write_status(ctx->out->ctx,  200);
		ctx->out->f->write_header(ctx->out->ctx,  kgl_header_content_type, _KS("text/plain"));
		ctx->out->f->write_header(ctx->out->ctx, kgl_header_content_length, (char *)&content_length, KGL_HEADER_VALUE_INT64);
		ctx->out->f->write_header(ctx->out->ctx,  kgl_header_cache_control, _KS("no-cache, no-store"));
		result = ctx->out->f->write_header_finish(ctx->out->ctx, content_length, &body);
		if (result != KGL_OK) {
			return result;
		}
		return body.f->close(body.ctx, body.f->write(body.ctx, _KS("test_upstream_ok")));
	}
	case test_sendfile_report: {
		kgl_response_body body;
		ctx->out->f->write_status(ctx->out->ctx,  200);
		ctx->out->f->write_header(ctx->out->ctx,  kgl_header_content_type, _KS("text/plain"));
		ctx->out->f->write_header(ctx->out->ctx,  kgl_header_cache_control, _KS("no-cache, no-store"));
		result = ctx->out->f->write_header_finish(ctx->out->ctx, -1, &body);
		if (result != KGL_OK) {
			return result;
		}
		std::stringstream s;
		s <<  katom_get((void*)&sendfile_total_count) << "\r\n";
		s <<  katom_get64((void*)&sendfile_total_length) << "\r\n";
		return body.f->close(body.ctx, body.f->write(body.ctx, s.str().c_str(), (int)s.str().size()));
	}
	default:
		break;
	}
	result = ctx->f->open_next(r, ctx->cn, ctx->in, ctx->out, NULL);
	ctx->in = NULL;
	ctx->out = NULL;
	return result;
}

static kgl_upstream async_upstream = {
	sizeof(kgl_upstream),0,
	"async_test",
	create_ctx,
	free_ctx,
	open,
	NULL
};
static kgl_upstream before_cache_async_upstream = {
	sizeof(kgl_upstream),KGL_UPSTREAM_BEFORE_CACHE,
	"async_test",
	create_ctx,
	free_ctx,
	open,
	NULL
};
void register_global_async_upstream(kgl_dso_version *ver)
{
	KGL_REGISTER_UPSTREAM(ver, &async_upstream);
}
void register_async_upstream(KREQUEST r, kgl_access_context *ctx, test_context *model_ctx, bool before_cache)
{
	ctx->f->support_function(r, ctx->cn, KF_REQ_UPSTREAM, before_cache ?&before_cache_async_upstream :&async_upstream, (void **)&model_ctx);
}
