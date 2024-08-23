#ifndef KHTTPFIBER_H
#define KHTTPFIBER_H
#include "ksapi.h"
#include "global.h"
#include "KHttpServer.h"
#include "KPathHandler.h"
#include "KHttpObject.h"
#include "KHttpRequest.h"
#include "KModel.h"

class KAutoBuffer;
class KHttpObject;
class KFetchObject;
class KRequestQueue;
class KUpstream;
class KSink;


namespace kangle {
	bool is_upstream_tcp(Proto_t proto);
	kgl_connection_result kgl_on_new_connection(kconnection* cn);
};
void start_request_fiber(KSink* sink, int header_length);
KGL_RESULT fiber_http_start(KHttpRequest *rq);
KGL_RESULT send_http2(KHttpRequest *rq, KHttpObject *obj, uint16_t status_code, KAutoBuffer *body=NULL);
KGL_RESULT send_error2(KHttpRequest *rq, int code, const char* reason);
KGL_RESULT handle_error(KHttpRequest *rq, int code, const char *msg);
KGL_RESULT stageHttpManage(KHttpRequest *rq);
//KGL_RESULT on_upstream_finished_header(KHttpRequest *rq);
KGL_RESULT on_upstream_finished_header(KHttpRequest* rq, kgl_response_body* body);
KGL_RESULT prepare_write_body(KHttpRequest* rq, kgl_response_body* body);
KGL_RESULT handle_denied_request(KHttpRequest* rq, KSafeSource fo);
KGL_RESULT open_queued_fetchobj(KHttpRequest* rq, KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, KRequestQueue* queue);

KGL_RESULT response_cache_object(KHttpRequest* rq, KHttpObject* obj);
KGL_RESULT prepare_request_fetchobj(KHttpRequest* rq,kgl_input_stream *in, kgl_output_stream *out);
KGL_RESULT process_upstream_no_body(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);

/* merge obj precondition to sub_request */
void merge_precondition(KHttpRequest* rq, KHttpObject* obj);
/* return true 200/206 false 304/412 */
inline bool kgl_request_precondition(KHttpRequest* rq, KHttpObject* obj) {
	kgl_precondition_flag flag;
	kgl_precondition* condition = rq->sink->get_precondition(&flag);
	kgl_request_range* range = rq->sink->data.range;
	/**
	* @rfc9110  13.2.2. Precedence of Preconditions
	* cache server ignore step 1 & 2
	*/
	/* step 3 */
	if (KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_none_match && condition) {
		if (!obj->precondition_entity(condition->entity->data, condition->entity->len)) {
			return false;
		}
		return true;
	}
	/* step 4 */
	if (KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_modified_since && condition) {
		if (!obj->precondition_time(condition->time)) {
			return false;
		}
	}
	return true;
}
/** kgl_match_if_range
* return true must response 206
* return false must response 200.
*/
inline bool kgl_match_if_range(kgl_precondition_flag flag, kgl_request_range* range, time_t last_modified) {
	if (!range->if_range_entity) {
		return true;
	}
	if (!KBIT_TEST(flag, kgl_precondition_if_range_date)) {
		return false;
	}
	return range->if_range_date == last_modified;
}
inline bool kgl_match_if_range(kgl_precondition_flag flag, kgl_request_range* range, kgl_len_str_t* etag) {
	if (!range->if_range_entity) {
		return true;
	}
	if (KBIT_TEST(flag, kgl_precondition_if_range_date)) {
		return false;
	}
	return kgl_mem_same(etag->data, etag->len, range->if_range_entity->data, range->if_range_entity->len);
}
/* return true 206, false 200 */
inline bool kgl_request_match_if_range(KHttpRequest* rq, KHttpObject* obj) {
	kgl_request_range* range = rq->sink->data.range;
	if (!range) {
		return false;
	}
	if (!obj->data->etag) {
		return !range->if_range_entity;
	}
	if (obj->data->i.condition_is_time) {
		return kgl_match_if_range(rq->sink->get_precondition_flag(), range, obj->data->last_modified);
	}
	return kgl_match_if_range(rq->sink->get_precondition_flag(), range, obj->data->etag);
}

bool process_check_final_source(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out, KGL_RESULT* result);
typedef KGL_RESULT(*kgl_request_handler)(kgl_str_t *path, void *data, KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
/**
* 发送在内存中的object.
*/
inline KGL_RESULT send_memory_object(KHttpRequest* rq) {
	KHttpObject* obj = rq->ctx.obj;
	if (!kgl_request_match_if_range(rq, obj)) {
		rq->sink->data.range = nullptr;
	}
	return response_cache_object(rq, obj);
}
template <typename CMP>
KGL_RESULT handle_request(KPathHandler<kgl_request_handler, CMP>& handler, void *data, KHttpRequest* rq,  kgl_input_stream* in, kgl_output_stream* out) {
	auto url = rq->sink->data.url;
	kgl_str_t path;
	path.data = url->path;
	path.len = strlen(path.data);
	auto h = handler.find((const char **)&path.data, &path.len);
	if (!h) {
		return send_error2(rq, 404, "no such file.");
	}
	return h(&path, data, rq, in, out);
}
#endif
