#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
#include "http.h"
#include "KChunked.h"
#include "KHttpObject.h"
#include "kselectable.h"
#include "cache.h"
#include "KHttpFilterManage.h"
#include "KFilterContext.h"
#include "KContentTransfer.h"
#include "KSimulateRequest.h"
#include "KSboFile.h"
#include "HttpFiber.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
int bigobj_read_body_fiber(void* arg, int got)
{
	KGL_RESULT result = (KGL_RESULT)got;
	KHttpRequest* rq = (KHttpRequest*)arg;
	kassert(rq->bo_ctx);
	if (result == KGL_OK) {
		result = rq->bo_ctx->ReadBody(rq);
	}
	rq->bo_ctx->Close(rq);
	return stage_end_request(rq, result);
}
void KBigObjectContext::Close(KHttpRequest* rq)
{
	if (net_fiber) {
		kfiber_join(net_fiber, NULL);
		net_fiber = NULL;
	}
}
int KBigObjectContext::EndRequest(KHttpRequest* rq)
{
	kassert(net_fiber == kfiber_self());
	if (last_net_from >= 0) {
		gobj->data->sbo->CloseWrite(gobj, last_net_from);
		last_net_from = -1;
	}
	return 0;
}
KGL_RESULT KBigObjectContext::ReadBody(KHttpRequest* rq)
{
	KSharedBigObject* sbo = getSharedBigObject();
	sbo->OpenRead(gobj);
	KGL_RESULT result = KGL_OK;
	int block_length = conf.io_buffer;
	char* buf = (char*)malloc(block_length);
	int retried = 0;
	while (left_read > 0) {
		int got = sbo->Read(rq, gobj, buf, range_from, (int)KGL_MIN((INT64)block_length,left_read));
		//printf("left_read=[" INT64_FORMAT "],range_from=[" INT64_FORMAT "] got=[%d]\n", left_read, range_from, got);
		if (got == -2) {
			//需要重试
			if (retried > 0) {
				result = KGL_EIO;
				break;
			}
			retried++;
			continue;
		}
		if (got <= 0) {
			result = KGL_EIO;
			break;
		}
		if (!rq->WriteAll(buf, got)) {
			result = KGL_EIO;
			break;
		}
		range_from += got;
		left_read -= got;
		retried = 0;
	}
	sbo->CloseRead(gobj);
	free(buf);
	return result;
}
KGL_RESULT KBigObjectContext::StreamWrite(INT64 offset, const char* buf, int len)
{
	return getSharedBigObject()->Write(gobj, offset, buf, len);
}
KBigObjectContext::KBigObjectContext(KHttpObject* gobj)
{
	range_from = 0;
	left_read = -1;
	this->gobj = gobj;
	net_fiber = NULL;
	last_net_from = -1;
	gobj->addRef();
}
KBigObjectContext::~KBigObjectContext()
{
	kassert(net_fiber == NULL);
	gobj->release();
}
KGL_RESULT KBigObjectContext::Open(KHttpRequest* rq, bool create_flag)
{
	if (!KBIT_TEST(rq->sink->data.flags, RQ_HAVE_RANGE)) {
		rq->sink->data.range_from = 0;
		rq->sink->data.range_to = -1;
	}
	range_from = rq->sink->data.range_from;
	if (rq->sink->data.range_to < 0) {
		left_read = gobj->index.content_length - range_from;
	} else {
		left_read = rq->sink->data.range_to + 1 - range_from;
	}
	kassert(gobj);
	if (!create_flag) {
		return OpenCache(rq);
	}
	kassert(net_fiber == NULL);
	net_fiber = kfiber_ref_self(false);
	kfiber_create(bigobj_read_body_fiber, rq, (int)SendHeader(rq), conf.fiber_stack_size, NULL);
	return KGL_OK;
}

KGL_RESULT KBigObjectContext::SendHeader(KHttpRequest* rq)
{
	if (rq->ctx->st == NULL) {
		rq->ctx->st = new KBigObjectStream(rq);
	}
	INT64 content_length = gobj->index.content_length;
	if (rq->needFilter()) {
		content_length = -1;
	}
	INT64 if_range_from = rq->sink->data.range_from;
	INT64 if_range_to = rq->sink->data.range_to;
	rq->sink->data.range_from = range_from;
	rq->sink->data.range_to = range_from + left_read - 1;
	if (rq->sink->data.range_to >= content_length - 1) {
		rq->sink->data.range_to = -1;
	}
	if (!build_obj_header(rq, gobj, content_length, range_from, left_read)) {
		return KGL_EUNKNOW;
	}
	rq->sink->data.range_from = if_range_from;
	rq->sink->data.range_to = if_range_to;
	//printf("start_send_header rq=[%p] left_read=[%lld]\n",rq,left_read);
	if (range_from < 0) {
		return KGL_NO_BODY;
	}
	//确保gobj==rq->ctx->obj
	assert(gobj == rq->ctx->obj);
	assert(range_from >= 0);
	if (rq->sink->data.meth == METH_HEAD) {
		return KGL_NO_BODY;
	}
	return KGL_OK;
}

KGL_RESULT KBigObjectContext::OpenCache(KHttpRequest* rq)
{
	assert(gobj);
	KSharedBigObject* sbo = getSharedBigObject();
	assert(sbo);
	if (sbo->CanSatisfy(rq, gobj)) {
#ifndef NDEBUG
		klog(KLOG_DEBUG, "st=%p can satisfy,from=" INT64_FORMAT ",to=" INT64_FORMAT "\n", rq, rq->sink->data.range_from, rq->sink->data.range_to);
#endif
		kfiber_next(bigobj_read_body_fiber, rq, (int)SendHeader(rq));
		return KGL_NEXT;
	}
#ifndef NDEBUG
	if (rq->sink->data.range_to >= 0) {
		assert(rq->sink->data.range_to >= rq->sink->data.range_from);
	}
#endif
	rq->ctx->push_obj(new KHttpObject(rq));
	rq->ctx->new_object = true;
	this->build_if_range(rq);
	return prepare_request_fetchobj(rq);
}
void KBigObjectContext::build_if_range(KHttpRequest* rq)
{
	//last_net_from = getSharedBigObject()->OpenWrite(rq->sink->data.range_from);
	KContext* context = rq->ctx;
	context->mt = modified_if_range_date;
	if (gobj->data->i.last_modified) {
		rq->sink->data.if_modified_since = gobj->data->i.last_modified;
	} else if (KBIT_TEST(gobj->index.flags, OBJ_HAS_ETAG)) {
		context->mt = modified_if_range_etag;
		KHttpHeader* h = gobj->findHeader("Etag", sizeof("Etag") - 1);
		if (h) {
			rq->sink->set_if_none_match(h->buf, h->val_len);
		}
	} else {
		rq->sink->data.if_modified_since = gobj->index.last_verified;
	}
	//提前更新last verified，减少源的连接数
	gobj->index.last_verified = kgl_current_sec;
}
void KBigObjectContext::StartNetRequest(KHttpRequest* rq, kfiber* fiber)
{
	if (net_fiber) {
		kfiber_join(net_fiber, NULL);
	}
	last_net_from = rq->sink->data.range_from;
	rq->ctx->old_obj = rq->ctx->obj;
	rq->ctx->obj = new KHttpObject(rq);
	rq->ctx->new_object = true;
	this->build_if_range(rq);
	net_fiber = fiber;
	kfiber_start(fiber, 0);
}
KGL_RESULT KBigObjectContext::upstream_recv_headed(KHttpRequest* rq, KHttpObject* obj, bool &bigobj_dead)
{
	assert(obj->data->i.type == MEMORY_OBJECT);
	bigobj_dead = false;
	if (obj->data->i.status_code != STATUS_CONTENT_PARTIAL) {
		//status_code not 206
		bigobj_dead = true;
		klog(KLOG_INFO, "bigobj_dead status_code=%d not expect 206\n", obj->data->i.status_code);
	} else if (!KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_RANGE)) {
		//没有Content-Range
		klog(KLOG_INFO, "bigobj_dead have no content_range\n");
		bigobj_dead = true;
	} else {
		KHttpObject* gobj = rq->bo_ctx->gobj;
		assert(gobj);
		if (obj->uk.url->encoding != gobj->uk.url->encoding) {
			if (gobj->IsContentEncoding() || obj->IsContentEncoding()) {
				bigobj_dead = true;
			} else {
				gobj->uk.url->merge_accept_encoding(obj->uk.url->encoding);
			}
		}
		if (obj->getTotalContentSize(rq) != gobj->index.content_length) {
			//内容长度有改变
			bigobj_dead = true;
			klog(KLOG_INFO, "obj content range is not eq %lld gobj=%lld\n", obj->getTotalContentSize(rq), gobj->index.content_length);
		}
	}
	if (!bigobj_dead) {
		rq->ctx->pop_obj();
		if (rq->ctx->st == NULL) {
			rq->ctx->st = new KBigObjectStream(rq);
		}
		if (net_fiber == NULL) {
			net_fiber = kfiber_ref_self(false);
			assert(last_net_from == -1);
			last_net_from = gobj->data->sbo->OpenWrite(rq->sink->data.range_from);
			kfiber_create(bigobj_read_body_fiber, rq, (int)SendHeader(rq), conf.fiber_stack_size, NULL);
		}
		return KGL_OK;
	}
	rq->ctx->cache_hit_part = false;
	if (net_fiber) {
		assert(net_fiber == kfiber_self());
		return KGL_EIO;
	}
	//大物件If-Range的请求回应,如果回应码不是206，则把大物件die掉。
	//或者是无if-range的请求，但回应不是304
	KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
	kassert(rq->tr == NULL);
	if (rq->ctx->st) {
		delete rq->ctx->st;
		rq->ctx->st = NULL;
	}
	delete rq->bo_ctx;
	rq->bo_ctx = NULL;
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
	}
	return KGL_OK ;
}
#endif//}}
