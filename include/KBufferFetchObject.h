#ifndef KBUFFERFETCHOBJECT_H
#define KBUFFERFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
#include "KHttpRequest.h"

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
		rq->response_connection();
		rq->response_content_length(length);
		rq->start_response_body(length);
		if (rq->sink->data.meth != METH_HEAD && header) {
			return rq->write_buf(header,length);
		}
		return KGL_OK;
	}
private:
	kbuf* header;
	int length;
};
#endif
