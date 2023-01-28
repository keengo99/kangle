#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
#include "http.h"
#include "KHttpObject.h"
#include "kselectable.h"
#include "cache.h"
#include "KFilterContext.h"
#include "KSimulateRequest.h"
#include "KSboFile.h"
#include "HttpFiber.h"
#include "KHttpTransfer.h"
#include "KDefer.h"

#ifdef ENABLE_BIG_OBJECT_206
int bigobj_send_fiber(void* arg, int got)
{
	KBigObjectContext* bo_ctx = (KBigObjectContext*)arg;
	return (int)bo_ctx->send_data(NULL);
}
KGL_RESULT KBigObjectContext::close(KGL_RESULT result)
{
	if (opened_body && rq->ctx.body.ctx) {
		result = rq->ctx.body.f->close(rq->ctx.body.ctx, result);
	}
	delete this;
	return result;
}
void KBigObjectContext::close_write() {
	if (write_offset >= 0) {
		obj->data->sbo->close_write(obj, write_offset);
		write_offset = -1;
	}
}
KGL_RESULT KBigObjectContext::send_data(bool *net_fiber)
{	
	if (!rq->ctx.body.ctx) {
		opened_body = true;
		int64_t body_size = this->left_read;
		bool build_status = true;
		kgl_load_cache_response_body(rq, &body_size);
		if (rq->sink->data.range) {
			if (!KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
				build_status = false;
				rq->response_status(STATUS_CONTENT_PARTIAL);
				rq->response_content_range(rq->sink->data.range, obj->index.content_length);
			}
		}
		if (!build_obj_header(rq, obj, body_size, build_status)) {
			return KGL_EUNKNOW;
		}
	}
	KGL_RESULT result = prepare_write_body(rq, &rq->ctx.body);
	if (result != KGL_OK) {
		return result;
	}
	KSharedBigObject* sbo = get_shared_big_obj();
	sbo->open_read(obj);
	int block_length = conf.io_buffer;
	char* buf = (char*)malloc(block_length);
	int retried = 0;
	while (left_read > 0) {
		int got = sbo->read(rq, obj,read_offset, buf, (int)KGL_MIN((int64_t)block_length,left_read), net_fiber);	
		//printf("left_read=[" INT64_FORMAT "],range_from=[" INT64_FORMAT "] got=[%d]\n", left_read, read_offset, got);
		if (got == -2) {
			if (net_fiber == NULL) {
				//treat as ok
				break;
			}
			if (net_fiber && *net_fiber) {
				write_offset = rq->ctx.sub_request->range->from;
				result = KGL_ENO_DATA;
				break;
			}
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
		result = rq->ctx.body.f->write(rq->ctx.body.ctx, buf, got);
		if (result!=KGL_OK) {
			break;
		}
		read_offset += got;
		left_read -= got;
		retried = 0;
	}
	sbo->close_read(obj);
	free(buf);
	return result;
}
bool KBigObjectContext::create_send_fiber(kfiber** send_fiber) {
	return 0 == kfiber_create(bigobj_send_fiber, this, 0, conf.fiber_stack_size, send_fiber);
}
KGL_RESULT KBigObjectContext::stream_write(INT64 offset, const char* buf, int len)
{
	return get_shared_big_obj()->write(obj, offset, buf, len);
}
KBigObjectContext::KBigObjectContext(KHttpRequest *rq, KHttpObject* obj)
{
	this->obj = obj;
	this->rq = rq;
	obj->addRef();
}
KBigObjectContext::~KBigObjectContext()
{
	obj->release();
}
KGL_RESULT KBigObjectContext::create()
{
	if (!adjust_length()) {
		return KGL_ENO_DATA;
	}
	return KGL_OK;
}
KGL_RESULT KBigObjectContext::open_cache()
{
	assert(obj);
	KSharedBigObject* sbo = get_shared_big_obj();
	assert(sbo);
	assert(!rq->ctx.sub_request);
	kgl_sub_request* srq = rq->alloc_sub_request();
	bool obj_is_expiration;
	bool match_if_range = kgl_request_match_if_range(rq,obj);
	KGL_RESULT result = KGL_OK;
	if (!match_if_range && !sbo->body_complete) {
		if (!verifiy_object(match_if_range, &result)) {
			return result;
		}
		//client is wrong if-none-match must send 200
		rq->sink->data.range = nullptr;
		obj_is_expiration = false;
	} else {
		obj_is_expiration = check_object_expiration(rq, obj);
	}
	srq->range = rq->sink->alloc<kgl_request_range>();
	if (rq->sink->data.range) {
		left_read = obj->index.content_length;
		kgl_adjust_range(rq->sink->data.range, &left_read);
		memcpy(srq->range, rq->sink->data.range, sizeof(kgl_sub_request));
	} else {
		memset(srq->range, 0, sizeof(kgl_sub_request));
		srq->range->to = -1;
	}
	kgl_satisfy_status status;
	if (rq->sink->data.meth == METH_HEAD) {
		status = kgl_satisfy_all;
	} else {
		status = sbo->can_satisfy(srq->range, obj);
	}
	switch (status) {
	case kgl_satisfy_all:
		if (obj_is_expiration) {
			if (!verifiy_object(false, &result)) {
				return result;
			}
		}
		//没有过期，并且也满足，直接走大文件缓存通道。
		if (kgl_request_precondition(rq, obj)) {
			return response_cache_object(rq,obj);
		}
		return send_http2(rq, obj, STATUS_NOT_MODIFIED);
	case kgl_satisfy_part:
		rq->ctx.cache_hit_part = true;
		break;
	default:
		KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
		break;
	}
	if (!adjust_length()) {
		return send_http2(rq, obj, STATUS_RANGE_NOT_SATISFIABLE, nullptr);
	}
	return internal_fetch();
}
bool KBigObjectContext::verifiy_object(bool match_if_range,KGL_RESULT *result)
{
	if (verified_object) {
		return true;
	}
	verified_object = true;
	merge_precondition(rq, obj);
	if (!match_if_range) {
		rq->ctx.sub_request->range = rq->sink->data.range;
		if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_DATE)) {
			rq->ctx.precondition_flag |= kgl_precondition_if_range_date;
		}
	}
	rq->push_obj(new KHttpObject(rq));
	kgl_input_stream in;
	kgl_output_stream out;
	new_default_stream(rq, &in, &out);
	defer(in.f->body.close(in.body_ctx); out.f->close(out.ctx));
	KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
	*result = prepare_request_fetchobj(rq, &in, &out);
	if (*result != KGL_NO_BODY) {
		rq->dead_old_obj();
		return false;
	}
	if (rq->ctx.obj->data->etag && !rq->ctx.obj->is_same_precondition(obj)) {
		rq->dead_old_obj();
		*result = process_upstream_no_body(rq, &in, &out);
		return false;
	}
	KBIT_SET(rq->sink->data.flags, RQ_OBJ_VERIFIED|RQ_CACHE_HIT);
	rq->ctx.old_obj->index.last_verified = kgl_current_sec;
	rq->ctx.old_obj->UpdateCache(rq->ctx.obj);
	rq->pop_obj();
	return true;
}
KGL_RESULT KBigObjectContext::internal_fetch()
{
	KGL_RESULT result = KGL_OK;
	for (;;) {
		rq->push_obj(new KHttpObject(rq));
		this->build_if_range(rq);

		kgl_input_stream in;
		kgl_output_stream out;
		new_default_stream(rq, &in, &out);
		tee_output_stream(&out);
		defer(in.f->body.close(in.body_ctx); out.f->close(out.ctx));
		if (!process_check_final_source(rq, &in, &out, &result)) {
			return result;
		}
		result = process_request_stream(rq, &in, &out);
		if (bigobj_dead) {
			return result;
		}
		if (result != KGL_OK) {
			break;
		}
		if (left_read <= 0) {
			break;
		}
		bool net_fiber = false;
		result = send_data(&net_fiber);
		if (result == KGL_OK) {
			break;
		}
		if (!net_fiber) {
			break;
		}
	}
	return result;
}
void KBigObjectContext::build_if_range(KHttpRequest* rq)
{
	assert(rq->ctx.sub_request && rq->ctx.sub_request->range);
	rq->ctx.sub_request->precondition = nullptr;
	rq->ctx.precondition_flag = 0;
	kgl_len_str_t* etag = obj->data->get_etag();
	if (etag) {
		rq->ctx.sub_request->range->if_range_entity = etag;
		if (etag->len > 2 && (*(etag->data) == 'W' || *(etag->data) == 'w') && (*(etag->data + 1) == '/')) {
			//weak etag
			rq->ctx.sub_request->range = nullptr;
		}
	} else {
		time_t last_modified = obj->data->get_last_modified();
		if (last_modified > 0) {
			KBIT_SET(rq->ctx.precondition_flag, kgl_precondition_if_range_date);
			rq->ctx.sub_request->range->if_range_date = last_modified;
		}
	}
	//提前更新last verified，减少源的连接数
	obj->index.last_verified = kgl_current_sec;
}
KGL_RESULT KBigObjectContext::upstream_recv_headed()
{
	assert(rq->ctx.old_obj && rq->ctx.old_obj->data->i.type == BIG_OBJECT_PROGRESS);
	assert(rq->ctx.old_obj == this->obj);
	KHttpObject* obj = rq->ctx.obj;
	assert(obj->data->i.type == MEMORY_OBJECT);
	assert(bigobj_dead == false); 
	if (obj->data->i.status_code != STATUS_CONTENT_PARTIAL) {
		//status_code not 206
		bigobj_dead = true;
		klog(KLOG_INFO, "bigobj_dead status_code=%d not expect 206\n", obj->data->i.status_code);
	} else if (!KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_RANGE)) {
		//没有Content-Range
		klog(KLOG_INFO, "bigobj_dead have no content_range\n");
		bigobj_dead = true;
	} else {
		assert(this->obj);
		if (obj->uk.url->encoding != this->obj->uk.url->encoding) {
			if (this->obj->IsContentEncoding() || obj->IsContentEncoding()) {
				bigobj_dead = true;
			} else {
				this->obj->uk.url->merge_accept_encoding(obj->uk.url->encoding);
			}
		}
		if (obj->getTotalContentSize() != this->obj->index.content_length) {
			//内容长度有改变
			bigobj_dead = true;
			klog(KLOG_INFO, "obj content range is not eq %lld gobj=%lld\n", obj->getTotalContentSize(), this->obj->index.content_length);
		}
	}
	if (bigobj_dead) {
		obj_dead();
		if (rq->ctx.body.ctx) {
			return KGL_EDATA_FORMAT;
		}
	} else {
		rq->pop_obj();
	}
	return KGL_OK ;
}
static KGL_RESULT bigobj_error(kgl_output_stream_ctx* ctx, uint16_t status_code, const char* reason, size_t reason_len) {
	KBigObjectContext* bo_ctx = (KBigObjectContext*)ctx;
	bo_ctx->bigobj_dead = true;
	bo_ctx->obj_dead();
	return KGL_EUNKNOW;
}
static KGL_RESULT upstream_recv_headed(kgl_output_stream_ctx* ctx,int64_t body_size, kgl_response_body* body) {
	KBigObjectContext* bo_ctx = (KBigObjectContext*)ctx;
	assert(bo_ctx->rq->ctx.old_obj && bo_ctx->rq->ctx.old_obj == bo_ctx->obj);
	kgl_default_output_stream_ctx* default_ctx = (kgl_default_output_stream_ctx*)bo_ctx->down_stream.ctx;
	assert(default_ctx->rq == bo_ctx->rq);
	default_ctx->parser_ctx.end_parse(bo_ctx->rq, body_size);
	assert(bo_ctx->rq->sink->data.meth != METH_HEAD);
	if (bo_ctx->rq->sink->data.meth == METH_HEAD) {
		//没有http body的情况
		return KGL_NO_BODY;
	}
	KGL_RESULT result = bo_ctx->upstream_recv_headed();
	if (result != KGL_OK) {
		return result;
	}
	if (bo_ctx->bigobj_dead) {
		assert(bo_ctx->write_offset == -1);
		return on_upstream_finished_header(bo_ctx->rq, body);
	}
	auto st = get_bigobj_response_body(bo_ctx->rq, body);
	assert(bo_ctx->rq->ctx.sub_request);
	st->bo_ctx = bo_ctx;
	st->offset = bo_ctx->rq->ctx.sub_request->range ? bo_ctx->rq->ctx.sub_request->range->from : 0;
	if (bo_ctx->write_offset == -1) {
		bo_ctx->write_offset = bo_ctx->get_shared_big_obj()->open_write(bo_ctx->obj, st->offset);
	}
	return result;
}
KGL_RESULT bigobj_write_trailer(kgl_output_stream_ctx* ctx, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	KBigObjectContext* bo_ctx = (KBigObjectContext*)ctx;
	if (bo_ctx->bigobj_dead) {
		return forward_write_trailer(ctx, attr, attr_len, val, val_len);
	}
	return KGL_OK;
}
void bigobj_release(kgl_output_stream_ctx* ctx) {
	KBigObjectContext* bo_ctx = (KBigObjectContext*)ctx;
	bo_ctx->down_stream.f->close(bo_ctx->down_stream.ctx);
	/* close_write 一般由KBigObjectReadContext 负责，但是有一些错误发生在KBigObjectReadContext 还未创建。*/
	bo_ctx->close_write();
	/* bigobj context not release here */
}
kgl_output_stream_function kgl_bigobj_output_stream_function = {
	//out header
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	bigobj_error,
	upstream_recv_headed,
	bigobj_write_trailer,
	bigobj_release
};
void KBigObjectContext::tee_output_stream(kgl_output_stream* out) {
	pipe_output_stream(this, &kgl_bigobj_output_stream_function, out);
}
#endif
