#ifndef KACCESSDSOSUPPORT_H
#define KACCESSDSOSUPPORT_H
#include "ksapi.h"
#include "KAutoBuffer.h"
#include "KHttpRequest.h"
void init_access_dso_support(kgl_access_context *ctx,int notify);
KGL_RESULT add_header_var(LPVOID buffer, LPDWORD size, KHttpHeader* header, const char* name, size_t len);
KGL_RESULT add_api_var(LPVOID buffer, LPDWORD size, const char *val, int len = 0);
KGL_RESULT get_request_variable(KHttpRequest *rq, KGL_VAR type, LPSTR  name, LPVOID  buffer, LPDWORD size);
KRequestQueue *get_request_queue(KHttpRequest *rq, const char *queue_name, int max_worker, int max_queue);
KRequestQueue *get_request_queue(KHttpRequest *rq, const char *queue_name);
KGL_RESULT base_support_function(KHttpRequest *rq, KF_REQ_TYPE req, PVOID data, PVOID *ret);
class KAccessContext
{
public:
	KAccessContext()
	{
		buffer = NULL;
	}
	~KAccessContext()
	{
		if (buffer) {
			delete buffer;
		}
	}
	KAutoBuffer *GetBuffer(KHttpRequest *rq)
	{
		if (buffer == NULL) {
			buffer = new KAutoBuffer(rq->sink->pool);
		}
		return buffer;
	}
	KAutoBuffer *buffer;
};
#endif
