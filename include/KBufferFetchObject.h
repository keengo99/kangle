#ifndef KBUFFERFETCHOBJECT_H
#define KBUFFERFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KDefer.h"

class KBufferFetchObject : public KFetchObject {
public:
	KBufferFetchObject(kbuf *buffer,int length)
	{
		header = buffer;
		this->length = length;
	}
	~KBufferFetchObject()
	{
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
	{
		out->f->write_header(out->ctx, kgl_header_content_length, (char *)&length, KGL_HEADER_VALUE_INT64);
		kgl_response_body body;
		KGL_RESULT result = out->f->write_header_finish(out->ctx, &body);
		if (result != KGL_OK) {
			return result;
		}
		return body.f->close(body.ctx, kgl_write_buf(&body, header, (int)length));
	}
private:
	kbuf* header;
	int64_t length;
};
#endif
