#ifndef KPUSHGATE_H_99
#define KPUSHGATE_H_99
#include "ksapi.h"
class KHttpRequest;
struct kgl_forward_stream : public kgl_output_stream {
	kgl_output_stream* down_stream;

};
kgl_input_stream *new_default_input_stream();
kgl_output_stream *new_default_output_stream();
kgl_output_stream *new_dechunk_stream(kgl_output_stream *down_gate);
kgl_forward_stream*new_forward_stream(kgl_output_stream*down_gate);
kgl_output_stream *get_check_output_stream();
kgl_input_stream* get_check_input_stream();

void forward_write_status(kgl_output_stream* gate, KREQUEST r, uint16_t status_code);
KGL_RESULT forward_write_body(kgl_output_stream* gate, KREQUEST r, const char* buf, int len);
KGL_RESULT forward_write_header_finish(kgl_output_stream* gate, KREQUEST r);
KGL_RESULT forward_write_message(kgl_output_stream* gate, KREQUEST rq, KGL_MSG_TYPE type, const void* msg, int32_t msg_flag);
KGL_RESULT forward_write_end(kgl_output_stream* out, KREQUEST rq, KGL_RESULT result);
KGL_RESULT forward_write_header(kgl_output_stream* gate, KREQUEST r, kgl_header_type attr, const char* val, int val_len);
KGL_RESULT forward_write_unknow_header(kgl_output_stream* gate, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);
KGL_RESULT forward_write_trailer(kgl_output_stream* gate, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len);

class KAutoReleaseStream
{
public:
	KAutoReleaseStream(kgl_input_stream *in, kgl_output_stream *out)
	{
		this->in = in;
		this->out = out;
	}
	~KAutoReleaseStream()
	{
		in->f->release(in);
		out->f->release(out);
	}
private:
	kgl_input_stream *in;
	kgl_output_stream *out;
};
#endif
