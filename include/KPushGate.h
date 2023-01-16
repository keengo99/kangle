#ifndef KPUSHGATE_H_99
#define KPUSHGATE_H_99
#include "ksapi.h"
#include "KStream.h"

class KHttpRequest;

struct kgl_forward_input_stream
{
	kgl_input_stream up_stream;
};
struct kgl_forward_output_stream
{
	kgl_output_stream down_stream;
};
struct kgl_forward_body
{
	kgl_response_body down_body;
};

template<KGL_RESULT(*forward_write)(kgl_response_body_ctx* , const char* , int)>
KGL_RESULT unsupport_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc) {
	for (int i = 0; i < bc; i++) {
		KGL_RESULT result = forward_write(ctx, bufs[i].iov_base, bufs[i].iov_len);
		if (result != KGL_OK) {
			return result;
		}
	}
	return KGL_OK;
}
void new_default_input_stream(KHttpRequest* rq, kgl_input_stream* in);
void new_default_output_stream(KHttpRequest* rq, kgl_output_stream* out);
void new_default_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);

bool new_dechunk_stream(kgl_output_stream* down_gate);
void pipe_response_body(kgl_forward_body* forward_body, kgl_response_body_function* f, kgl_response_body* down_body);
void pipe_output_stream(kgl_forward_output_stream* forward_st, kgl_output_stream_function* f, kgl_output_stream* down_stream);
void pipe_input_stream(kgl_forward_input_stream* forward_st, kgl_input_stream_function* f, kgl_input_stream* up_stream);
void get_check_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
void forward_write_status(kgl_output_stream_ctx* gate, uint16_t status_code);

kgl_url* forward_get_url(kgl_input_stream_ctx* ctx);
int forward_get_header_count(kgl_input_stream_ctx* ctx);
kgl_precondition* forward_get_precondition(kgl_input_stream_ctx* ctx, kgl_precondition_flag* flag);
kgl_request_range *forward_get_range(kgl_input_stream_ctx* ctx);
KGL_RESULT forward_get_header(kgl_input_stream_ctx* ctx, kgl_parse_header_ctx* parse_ctx, kgl_parse_header_function* parse);
KGL_RESULT forward_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc);
KGL_RESULT forward_write(kgl_response_body_ctx* gate, const char* buf, int len);
KGL_RESULT forward_write_header_finish(kgl_output_stream_ctx* gate, kgl_response_body* body);
KGL_RESULT forward_error(kgl_output_stream_ctx* gate, uint16_t status_code, const char* msg, size_t msg_len);
KGL_RESULT forward_close(kgl_response_body_ctx* out, KGL_RESULT result);
KGL_RESULT forward_write_header(kgl_output_stream_ctx* gate, kgl_header_type attr, const char* val, int val_len);
KGL_RESULT forward_write_unknow_header(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
KGL_RESULT forward_write_trailer(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
KGL_RESULT unsupport_sendfile(kgl_response_body_ctx* out, KASYNC_FILE fp, int64_t* len);
bool support_sendfile_false(kgl_response_body_ctx* out);
KGL_RESULT kgl_empty_flush(kgl_response_body_ctx* out);
KGL_RESULT forward_sendfile(kgl_response_body_ctx* out, KASYNC_FILE fp, int64_t* len);
bool forward_support_sendfile(kgl_response_body_ctx* out);
KGL_RESULT forward_flush(kgl_response_body_ctx* out);

void get_default_response_body(KREQUEST r, kgl_response_body* body);
#endif
