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
	if (!obj->data->sbo->create(obj)) {
		return KGL_EIO;
	}
	obj->dc_index_update = 1;
	rq->store_obj();
	assert(obj->list_state != LIST_IN_NONE);
	if (obj->list_state != LIST_IN_NONE) {
		dci->start(ci_add, obj);
	}
	KBigObjectContext* bo_ctx = new KBigObjectContext(rq, obj);
	auto result = bo_ctx->create();
	if (result != KGL_OK) {
		return result;
	}
	
	assert(!rq->ctx.st.ctx);
	auto st = get_tunon_bigobj_response_body(rq, body);
	st->bo_ctx = bo_ctx;
	bo_ctx->write_offset = obj->data->sbo->open_write(obj, bo_ctx->read_offset);
	st->offset = bo_ctx->write_offset;
	return KGL_OK;
}
#endif
