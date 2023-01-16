#include "KBigObject.h"
#include "KHttpObject.h"
#include "KBigObjectStream.h"
#include "KFetchBigObject.h"
#include "http.h"
#include "KDiskCache.h"
#include "KDiskCacheIndex.h"
#include "cache.h"
#include "HttpFiber.h"

#ifdef ENABLE_BIG_OBJECT_206
KGL_RESULT turn_on_bigobject(KHttpRequest* rq, KHttpObject* obj, kgl_response_body* body) {
	assert(rq->ctx.st.ctx == nullptr);
	rq->dead_old_obj();
	kassert(obj->data->i.type == MEMORY_OBJECT);
	kassert(!obj->in_cache);
	kassert(obj->refs == 1);
	kassert(obj->data->bodys == NULL);
	obj->data->i.type = BIG_OBJECT_PROGRESS;
	obj->data->sbo = new KSharedBigObject;
	if (obj->data->i.status_code == STATUS_OK) {
		rq->sink->data.range = nullptr;
	}
	if (!obj->data->sbo->open(obj, true)) {
		return KGL_EIO;
	}
	assert(false);
	INT64 last_net_from = obj->data->sbo->open_write(0);
	//printf("range_from=[%d] last_net_from=[%d]\n", (int)rq->sink->data.range_from, (int)last_net_from);
	//assert(last_net_from == -1 || last_net_from == rq->sink->data.range_from);
	obj->dc_index_update = 1;
	rq->store_obj();
	assert(obj->list_state != LIST_IN_NONE);
	if (obj->list_state != LIST_IN_NONE) {
		dci->start(ci_add, obj);
	}
	KBigObjectContext *bo_ctx = new KBigObjectContext(rq, obj);

	rq->registerRequestCleanHook([](void* ctx) {
		KBigObjectContext* bo_ctx = (KBigObjectContext*)ctx;
		bo_ctx->close();
	},bo_ctx);

	bo_ctx->create();
	assert(!rq->ctx.st.ctx);
	KGL_RESULT result = prepare_write_body(rq, &rq->ctx.st);
	if (result != KGL_OK) {
		bo_ctx->close_write(last_net_from);
		return result;
	}
	auto st = get_bigobj_response_body(rq, body);	
	st->bo_ctx = bo_ctx;
	st->write_from = last_net_from;
	assert(st->write_from == st->offset);
	/*
	kgl_request_range* range = rq->sink->data.range;
	if (range) {
		send_ctx->offset = range->from;
		if (range->to < 0) {
			send_ctx->left_read = obj->index.content_length - range->from;
		} else {
			send_ctx->left_read = range->to + 1 - range->from;
		}
	} else {
		send_ctx->offset = 0;
		send_ctx->left_read = obj->index.content_length;
	}
	kfiber_create(bigobj_send_fiber, send_ctx, 1, conf.fiber_stack_size, &st->wait_fiber);
	*/
	return result;
}
#endif
