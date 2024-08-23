#ifndef KHTTPOBJECTPARSER_H_
#define KHTTPOBJECTPARSER_H_
#include "KHttpObject.h"
class KHttpResponseParser : public KHttpHeaderManager {
public:
	KHttpResponseParser()
	{
		memset(this, 0, sizeof(*this));
	}
	~KHttpResponseParser()
	{
		if (header) {
			free_header_list(header);
		}
	}
	bool parse_header(KHttpRequest* rq, kgl_header_type attr, const char* val, int val_len);
	bool parse_unknow_header(KHttpRequest *rq, const char *attr, int attr_len, const char *val, int val_len, bool request_line);
	void end_parse(KHttpRequest *rq,int64_t body_size);
	void commit_headers(KHttpObject *obj) {
		if (last) {
			last->next = obj->data->headers;
			obj->data->headers = steal_header();
			last = nullptr;
		}
	}
private:
	time_t serverDate;
	time_t expireDate;
	unsigned age;
};
kgl_header_type kgl_parse_response_header(const char* attr, hlen_t attr_len);
struct kgl_default_output_stream_ctx
{
	KHttpResponseParser parser_ctx;
	KHttpRequest* rq;
};
#endif /*KHTTPOBJECTPARSER_H_*/
