#ifndef KHTTPFIBER_H
#define KHTTPFIBER_H
#include "ksapi.h"
class KHttpRequest;
class KAutoBuffer;
class KHttpObject;
class KFetchObject;
class KRequestQueue;
int start_request_fiber(void *rq, int header_length);
KGL_RESULT fiber_http_start(KHttpRequest *rq);
KGL_RESULT send_http2(KHttpRequest *rq, KHttpObject *obj, uint16_t status_code, KAutoBuffer *body=NULL);
KGL_RESULT send_error2(KHttpRequest *rq, int code, const char* reason);
KGL_RESULT handle_error(KHttpRequest *rq, int code, const char *msg);
KGL_RESULT stageHttpManage(KHttpRequest *rq);
KGL_RESULT on_upstream_finished_header(KHttpRequest *rq);
KGL_RESULT handle_denied_request(KHttpRequest* rq);
//KGL_RESULT open_fetchobj(KHttpRequest* rq, KFetchObject* fo);
KGL_RESULT process_request(KHttpRequest* rq, KFetchObject* fo);
KGL_RESULT open_fetchobj(KHttpRequest* rq, KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out);
KGL_RESULT open_queued_fetchobj(KHttpRequest* rq, KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, KRequestQueue* queue);
KGL_RESULT send_memory_object(KHttpRequest* rq);
int stage_end_request(KHttpRequest* rq, KGL_RESULT result);
KGL_RESULT prepare_request_fetchobj(KHttpRequest* rq);
#endif
