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
	bool before_cache() override {
		return true;
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override
	{
		kgl_response_body body;
		KGL_RESULT result = out->f->write_header_finish(out->ctx, length, &body);
		if (result != KGL_OK) {
			return result;
		}
		return body.f->close(body.ctx, body.f->writev(body.ctx, header, length));
	}
private:
	kbuf* header;
	int length;
};
#endif
