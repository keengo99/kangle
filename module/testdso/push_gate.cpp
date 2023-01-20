#ifndef _WIN32
#include <sys/select.h>
#include <stddef.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "ksapi.h"
#include "upstream.h"

static void my_msleep(int msec) {
#if	defined(OSF)
	/* DU don't want to sleep in poll when number of descriptors is 0 */
	usleep(msec * 1000);
#elif	defined(_WIN32)
	Sleep(msec);
#else
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	select(1, NULL, NULL, NULL, &tv);
#endif
}
static void push_status(kgl_output_stream_ctx *gate, uint16_t status_code)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_status(ctx->out->ctx,  status_code);
}
static KGL_RESULT push_header(kgl_output_stream_ctx* gate,  kgl_header_type attr, const char *val, int val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_header(ctx->out->ctx, attr, val, val_len);
}
static KGL_RESULT push_unknow_header(kgl_output_stream_ctx* gate,  const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_unknow_header(ctx->out->ctx, attr, attr_len, val, val_len);
}
static KGL_RESULT push_header_finish(kgl_output_stream_ctx*gate, int64_t body_size, kgl_response_body *body) {
	kgl_async_context* ctx = kgl_get_out_async_context(gate);
	test_context* t = (test_context*)ctx->module;
	if (t->read_post) {
		ctx->out->f->write_unknow_header(ctx->out->ctx,  kgl_expand_string("x-testdso"), kgl_expand_string("forward-sleep"));
	}
	return ctx->out->f->write_header_finish(ctx->out->ctx,body_size, body);
}
static KGL_RESULT push_trailer(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_async_context* ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_trailer(ctx->out->ctx, attr, attr_len, val, val_len);
}
#if 0

static bool support_sendfile(KCONN out, KREQUEST rq) {
	kgl_async_context* ctx = kgl_get_out_async_context(out);
	return ctx->out->f->support_sendfile(ctx->out, rq);
}
static KGL_RESULT sendfile(kgl_output_stream* out, KREQUEST rq, KASYNC_FILE fp, int64_t *len) {
	kgl_async_context* ctx = kgl_get_out_async_context(out);
	return ctx->out->f->sendfile(ctx->out, rq, fp, len);
}

static KGL_RESULT push_body(kgl_output_stream *gate, KREQUEST rq, const char *str, int len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_body(ctx->out, rq, str, len);
}
static KGL_RESULT push_body_finish(kgl_output_stream *gate, KREQUEST rq, KGL_RESULT result)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_end(ctx->out, rq, result);
}
#endif
static KGL_RESULT handle_error(kgl_output_stream_ctx*gate,  uint16_t status_code, const char *msg, size_t msg_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->error(ctx->out->ctx, status_code,  msg, msg_len);
}

static kgl_output_stream_function push_gate_function = {
		push_status,
		push_header,
		push_unknow_header,
		handle_error,
		push_header_finish,
		push_trailer
};
