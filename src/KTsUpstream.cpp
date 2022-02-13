#include <stdio.h>
#include "KTsUpstream.h"
#include "kselectable.h"
#include "kconnection.h"
#include "kfiber.h"
#include "KConfig.h"
struct KTsUpstreamParam {
	KUpstream* us;
	union {
		WSABUF* bufs;
		char* buf;
		time_t base_time;
		KTsUpstream* ts;
	};
	union {
		int bc;
		int len;
		int life_time;
	};
};
extern void http2_header_callback(KOPAQUE data, void *arg, const char *attr, int attr_len, const char *val, int val_len);
static void ts_header_callback(KOPAQUE data, void *arg, const char *attr, int attr_len, const char *val, int val_len)
{
	KTsUpstream *ts = (KTsUpstream *)arg;
	kgl_pool_t *pool = ts->us->GetPool();
	KHttpHeader *header = new_pool_http_header(pool, attr, attr_len, val, val_len);
	if (ts->header) {
		ts->last_header->next = header;
		ts->last_header = header;
	} else {
		ts->header = header;
		ts->last_header = header;
	}
	return;
}
static int ts_shutdown(void* arg, int len)
{
	KUpstream* us = (KUpstream*)arg;
	us->Shutdown();
	return 0;
}
static int ts_write_end(void* arg, int len)
{
	KUpstream* us = (KUpstream*)arg;
	us->WriteEnd();
	return 0;
}
static int ts_gc(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	param->us->Gc(param->life_time, param->base_time);
	return 0;
}
static int ts_read(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	return param->us->Read(param->buf, param->len);
}
static int ts_write(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	return param->us->Write(param->bufs, param->len);
}
static int ts_read_http_header(void* arg, int len)
{
	KGL_RESULT result = KGL_ENOT_SUPPORT;
	bool ret = ((KUpstream*)arg)->ReadHttpHeader(&result);
	assert(ret);
	return (int)result;
}
static int ts_set_http2_parser(void* arg, int len)
{
	KTsUpstreamParam* param = (KTsUpstreamParam*)arg;
	param->us->SetHttp2Parser(param->ts, ts_header_callback);
	return 0;
}
static kev_result next_ts_destroy(KOPAQUE data, void *arg, int got)
{
	KUpstream *us = (KUpstream *)arg;
	us->Destroy();
	return kev_ok;
}

bool KTsUpstream::SetHttp2Parser(void *arg, kgl_header_callback header)
{
	assert(us->IsMultiStream());
	memset(&stack, 0, sizeof(KTsUpstreamStack));
	stack.arg = arg;
	stack.header = header;
	KTsUpstreamParam param;
	param.us = us;
	param.ts = this;

	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_set_http2_parser, &param, 0, conf.fiber_stack_size , &fiber) != 0) {
		return false;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return true;
}

bool KTsUpstream::ReadHttpHeader(KGL_RESULT* result)
{
	if (!us->IsMultiStream()) {
		return false;
	}
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_read_http_header, us, 0, conf.fiber_stack_size, &fiber) != 0) {
		*result = KGL_ESYSCALL;
		return true;
	}
	int ret;
	kfiber_join(fiber, &ret);
	while (header) {
		kassert(stack.header == http2_header_callback);
		stack.header(us->GetOpaque(), stack.arg, header->attr, header->attr_len, header->val, header->val_len);
		header = header->next;
	}
	*result = (KGL_RESULT)ret;
	return true;
}
int KTsUpstream::Write(WSABUF* buf, int bc)
{
	KTsUpstreamParam param;
	param.us = us;
	param.bufs = buf;
	param.len = bc;
	assert(buf[0].iov_len > 0);
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_write, &param, 0, 0, &fiber) != 0) {
		return -1;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return ret;
}
int KTsUpstream::Read(char* buf, int len)
{
	KTsUpstreamParam param;
	param.us = us;
	param.buf = buf;
	param.len = len;
	kfiber* fiber = NULL;
	if (kfiber_create2(us->GetSelector(), ts_read, &param, 0, 0, &fiber) != 0) {
		return -1;
	}
	int ret;
	kfiber_join(fiber, &ret);
	return ret;
}
void KTsUpstream::WriteEnd()
{
	if (!us->IsMultiStream()) {
		us->WriteEnd();
		return;
	}
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_write_end, us, 0, conf.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
}
void KTsUpstream::Shutdown()
{
	if (!us->IsMultiStream()) {
		us->Shutdown();
		return;
	}
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_shutdown, us, 0, conf.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
}
void KTsUpstream::Destroy()
{
	fprintf(stderr, "never goto here.\n");
	kassert(false);
	if (us) {
		selectable_next(&us->GetConnection()->st, next_ts_destroy, us, 0);
		us = NULL;
	}
	delete this;
}
bool KTsUpstream::IsLocked()
{
	return false;
}
void KTsUpstream::Gc(int life_time,time_t base_time)
{
	KTsUpstreamParam param;
	param.base_time = base_time;
	param.life_time = life_time;
	param.us = us;
	kfiber* fiber = NULL;
	kfiber_create2(us->GetSelector(), ts_gc, &param, 0, conf.fiber_stack_size, &fiber);
	int ret;
	kfiber_join(fiber, &ret);
	us = NULL;
	delete this;
}
