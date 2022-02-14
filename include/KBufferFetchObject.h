#ifndef KBUFFERFETCHOBJECT_H
#define KBUFFERFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
class KBufferFetchObject : public KFetchObject {
public:
	KBufferFetchObject(kbuf *buf,int offset,int len,kgl_pool_t *pool)
	{
		memset(&buffer, 0, sizeof(buffer));
		kassert(len > 0);
		kr_init(&buffer, buf, offset, len, pool);
	}
	KBufferFetchObject(KAutoBuffer *buffer)
	{
		memset(&this->buffer, 0, sizeof(buffer));
		kassert(buffer->getLen() > 0);
		kr_init(&this->buffer, buffer->getHead(), 0,buffer->getLen(), buffer->pool);			
	}
	~KBufferFetchObject()
	{
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
	{
		return KGL_EUNKNOW;
	}
	kr_buffer buffer;
};
#endif
