#ifndef KVECTOR_BUFFER_FETCHOBJECT_H
#define KVECTOR_BUFFER_FETCHOBJECT_H
#include "KFetchObject.h"
class KVectorBufferFetchObject : public KFetchObject {
public:
	KVectorBufferFetchObject(WSABUF *buf, int bc)
	{
		this->buf = buf;
		this->bc = bc;
	}
	int buffer(LPWSABUF buf, int bc)
	{
		int copy_bc = MIN(bc, this->bc);
		kgl_memcpy(buf, this->buf, copy_bc * sizeof(WSABUF));
		return copy_bc;
	}
	bool result(int got)
	{		
		while (got > 0) {
			assert(bc > 0);
			int this_got = MIN(got, (int)buf->iov_len);
			buf->iov_len -= this_got;
			got -= this_got;
			if (buf->iov_len == 0) {
				buf++;
				bc--;
			}
		}
		return bc > 0;
	}
	int64_t GetLen()
	{
		int64_t len = 0;
		for (int i = 0;i < bc;i++) {
			len += buf[i].iov_len;
		}
		return len;
	}
private:
	WSABUF *buf;
	int bc;
};
#endif
