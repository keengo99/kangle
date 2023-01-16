#include "KBigObjectStream.h"
#include "KPushGate.h"

#ifdef ENABLE_BIG_OBJECT_206

static KGL_RESULT bo_write(kgl_response_body_ctx* ctx, const char* buf, int size) {
	KBigObjectReadContext* st = (KBigObjectReadContext*)ctx;
	if (st->bo_ctx->read_fiber_error) {
		return KGL_EIO;
	}
	KGL_RESULT result = st->bo_ctx->stream_write(st->offset, buf, size);
	if (result == KGL_END) {
		result = KGL_OK;
	}
	if (result != KGL_OK) {
		st->bo_ctx->read_fiber_error = true;
	}
	st->offset += size;
	if (st->send_fiber == nullptr) {
		if (!st->bo_ctx->create_send_fiber(&st->send_fiber)) {
			klog(KLOG_ERR, "cann't create bigobject send_fiber\n");
			return KGL_ESYSCALL;
		}
		assert(st->send_fiber);
	} else {
		int ret;
		if (kfiber_try_join(st->send_fiber, &ret) == 0) {
			/* send fiber has exit. */
			st->send_fiber = nullptr;
			st->bo_ctx->read_fiber_error = true;
			if (result == KGL_OK) {
				return (KGL_RESULT)ret;
			}
		}
	}
	return result;
}
static KGL_RESULT bo_close_write(KBigObjectReadContext* st, KGL_RESULT result) {
	//printf("bo_close_write result=[%d]\n", result);
	st->bo_ctx->close_write();	
	if (st->send_fiber) {
		int ret;
		kfiber_join(st->send_fiber, &ret);
		if (result == KGL_OK) {
			result = (KGL_RESULT)ret;
		}
	}
	return result;
}
KGL_RESULT bo_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	KBigObjectReadContext* st = (KBigObjectReadContext*)ctx;
	result = bo_close_write(st, result);
	delete st;
	return result;
}
KGL_RESULT turn_on_bo_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	KBigObjectReadContext* st = (KBigObjectReadContext*)ctx;
	result = bo_close_write(st, result);
	st->bo_ctx->close(result);
	delete st;
	return result;
}
kgl_response_body_function bigobj_response_body_function = {
	unsupport_writev<bo_write>,
	bo_write,
	kgl_empty_flush,
	support_sendfile_false,
	unsupport_sendfile,
	bo_close
};
kgl_response_body_function turnon_bigobj_response_body_function = {
	unsupport_writev<bo_write>,
	bo_write,
	kgl_empty_flush,
	support_sendfile_false,
	unsupport_sendfile,
	turn_on_bo_close,
};
KBigObjectReadContext*get_bigobj_response_body(KHttpRequest *rq, kgl_response_body* body) {
	KBigObjectReadContext* bo_stream = new KBigObjectReadContext;
	memset(bo_stream, 0, sizeof(KBigObjectReadContext));
	body->ctx = (kgl_response_body_ctx *)bo_stream;
	body->f = &bigobj_response_body_function;
	return bo_stream;
}
KBigObjectReadContext* get_tunon_bigobj_response_body(KHttpRequest* rq, kgl_response_body* body) {
	KBigObjectReadContext* bo_stream = new KBigObjectReadContext;
	memset(bo_stream, 0, sizeof(KBigObjectReadContext));
	body->ctx = (kgl_response_body_ctx*)bo_stream;
	body->f = &turnon_bigobj_response_body_function;
	return bo_stream;
}
#endif
