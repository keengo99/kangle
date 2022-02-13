#include "KBigObject.h"
#include "KHttpObject.h"
#include "KBigObjectStream.h"
#include "KFetchBigObject.h"
#include "http.h"
#include "KDiskCache.h"
#include "KDiskCacheIndex.h"
#include "cache.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
//大物件处理,网络请求部分处理
KGL_RESULT handle_bigobject_progress(KHttpRequest* rq, KHttpObject* obj)
{
	return KGL_OK;
}
//由内存物件转入大物件
KGL_RESULT turn_on_bigobject(KHttpRequest *rq,KHttpObject *obj)
{
	rq->ctx->DeadOldObject();
	kassert(obj->data->type == MEMORY_OBJECT);
	kassert(rq->ctx->new_object);
	kassert(obj->refs==1);
	kassert(obj->data->bodys == NULL);
	/*
	删除httpobject->data->bodys
	*/
	//kbuf *bodys = obj->data->bodys;
	obj->data->type = BIG_OBJECT_PROGRESS;
	obj->data->sbo = new KSharedBigObject;
	if (obj->data->status_code == STATUS_OK) {
		CLR(rq->flags,RQ_HAVE_RANGE);
		rq->range_from = 0;
		rq->range_to = -1;
	}
	if (!obj->data->sbo->Open(rq, obj, true)) {
		return KGL_EIO;
	}
	INT64 last_net_from = obj->data->sbo->OpenWrite(rq->range_from);
	obj->dc_index_update = 1;
	rq->ctx->store_obj(rq);
	assert(obj->list_state != KGL_LIST_NONE);
	if (obj->list_state != KGL_LIST_NONE) {
		dci->start(ci_add, obj);
	}
	if (rq->bo_ctx==NULL) {
		rq->bo_ctx = new KBigObjectContext(obj);
	}
	KGL_RESULT result = rq->bo_ctx->Open(rq,true);
	rq->bo_ctx->last_net_from = last_net_from;
	if (last_net_from==-1) {
		return KGL_DOUBLICATE;
	}
	return result;
}
#endif
//}}
