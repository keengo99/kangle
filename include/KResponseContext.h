#ifndef KRESPONSECONTEXT_H
#define KRESPONSECONTEXT_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#include "log.h"
#ifdef ENABLE_ATOM
#include "katom.h"
#endif
#ifndef _WIN32
#include <sys/uio.h>
#endif
#include "kselector.h"
#include "KAutoBuffer.h"

class KResponseContext
{
public:
	KResponseContext(kgl_pool_t *pool) : ab(pool)
	{
		buffer = NULL;
		result = NULL;
	}
	bool ReadSuccess(int *got)
	{
		return ab.readSuccess(got);
	}
	int GetReadBuffer(KOPAQUE data, LPWSABUF buffer,int bc)
	{
		int ret = ab.getReadBuffer(buffer, bc);
		if (this->buffer && ret<bc) {
			//have body
			int body_ret = this->buffer(data, this->arg, buffer + ret, bc - ret);
			ret += body_ret;
		}
		return ret;
	}
	void head_insert_const(const char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool, sizeof(kbuf));
		t->data = (char *)str;
		t->used = len;
		ab.Insert(t);
	}
	void head_append(char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool,sizeof(kbuf));
		memset(t,0,sizeof(kbuf));
		t->data = str;
		t->used = len;
		ab.Append(t);
	}
	void head_append_const(const char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)kgl_pnalloc(ab.pool, sizeof(kbuf));
		t->data = (char *)str;
		t->used = len;
		ab.Append(t);
	}
	void SwitchRead() {
		ab.SwitchRead();
	}
	int GetLen()
	{
		return ab.getLen();
	}
	void *arg;
	buffer_callback buffer;
	result_callback result;
	friend class KHttpSink;
private:
	KAutoBuffer ab;
};
#endif
