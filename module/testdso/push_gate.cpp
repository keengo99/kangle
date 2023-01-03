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
static void push_status(kgl_output_stream *gate, KREQUEST rq, uint16_t status_code)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_status(ctx->out, rq, status_code);
}
static KGL_RESULT push_header(kgl_output_stream *gate, KREQUEST rq, kgl_header_type attr, const char *val, int val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_header(ctx->out, rq, attr, val, val_len);
}
static KGL_RESULT push_unknow_header(kgl_output_stream *gate, KREQUEST rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_unknow_header(ctx->out, rq, attr, attr_len, val, val_len);
}
static KGL_RESULT push_header_finish(kgl_output_stream *gate, KREQUEST rq)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	test_context *t = (test_context *)ctx->module;
	if (t->read_post) {
		ctx->out->f->write_unknow_header(ctx->out, rq, kgl_expand_string("x-testdso"), kgl_expand_string("forward-sleep"));
	}
	return ctx->out->f->write_header_finish(ctx->out, rq);
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
static KGL_RESULT handle_error(kgl_output_stream *gate, KREQUEST rq, KGL_MSG_TYPE type,const void *msg, int msg_flag)
{
	kgl_async_context *ctx = kgl_get_out_async_context(gate);
	return ctx->out->f->write_message(ctx->out, rq, type,  msg,msg_flag);
}
static kgl_output_stream_function push_gate_function = {
	push_status,
	push_header,
	push_unknow_header,
	push_header_finish,
	push_body,
	handle_error,
	push_body_finish,
	(void(*)(kgl_output_stream *))free
};

