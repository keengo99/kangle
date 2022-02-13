#ifndef KSENDBUFFER_H
#define KSENDBUFFER_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
class KSendBuffer
{
public:
	KSendBuffer()
	{
		memset(this,0,sizeof(*this));
	}
	~KSendBuffer()
	{
		clean();
	}
	void clean()
	{
		while (header) {			
			xfree(header->data);			
			kbuf *next = header->next;
			xfree(header);
			header = next;
		}
		memset(this,0,sizeof(*this));
	}
	void getReadBuffer(LPWSABUF buffer,int &bufferCount)
	{
		assert(hot);
		assert(header);
		kbuf *tmp = header;
		buffer[0].iov_base = hot;
		buffer[0].iov_len = header->used - (int)(hot - header->data);
		int i;
		for (i=1;i<bufferCount;i++) {
			tmp = tmp->next;
			if (tmp==NULL) {
				break;
			}
			buffer[i].iov_base = tmp->data;
			buffer[i].iov_len = tmp->used;
		}
		bufferCount = i;
	}
	char *getReadBuffer(int &len)
	{
		WSABUF buffer;
		int bufferCount = 1;
		getReadBuffer(&buffer,bufferCount);
		len = buffer.iov_len;
		return (char *)buffer.iov_base;
	}
	bool readSuccess(int got)
	{
		total_len -= got;
		while (got>0) {
			int hot_left = header->used - (int)(hot - header->data);
			int this_len = MIN(got,hot_left);
			hot += this_len;
			got -= this_len;
			if (header->used == hot - header->data) {
				kbuf *next = header->next;				
				xfree(header->data);					
				xfree(header);				
				header = next;
				if (header==NULL) {
					assert(got==0);
					assert(total_len == 0);
					last = NULL;
					hot = NULL;
					return false;
				}
				hot = header->data;
			}
		}
		return true;

	}
	void append(const char *str,uint16_t len)
	{
		char *data = (char *)malloc(len);
		kgl_memcpy(data,str,len);
		pushEnd(data,len);
	}
	void pushEnd(kbuf *buf)
	{
		add(buf,buf->used);
	}
	void insert(const char *str,uint16_t len)
	{
		char *data = (char *)malloc(len);
		kgl_memcpy(data,str,len);
		pushHead(data,len);
	}
	void pushHead(char *str,uint16_t len)
	{
		if (header==NULL) {
			pushEnd(str,len);
			return;
		}
		kbuf *t = (kbuf *)xmalloc(sizeof(kbuf));
		memset(t,0,sizeof(kbuf));
		t->data = str;
		t->used = len;
		t->next = header;
		assert(hot == header->data);
		header = t;
		hot = t->data;
		total_len += len;
	}
	void pushEnd(char *str,uint16_t len)
	{
		kbuf *t = (kbuf *)xmalloc(sizeof(kbuf));
		memset(t,0,sizeof(kbuf));
		t->data = str;
		t->used = len;
		add(t,len);
	}
	kbuf *getHeader()
	{
		return header;
	}
	int getLength() {
		return total_len;
	}
private:
	void add(kbuf *buf,int len)
	{
		total_len += len;
		if (last==NULL) {
			assert(header==NULL);
			last = header = buf;
			hot = header->data;
		} else {
			last->next = buf;
			last = buf;
		}
	}
	kbuf *last;
	kbuf *header;
	char *hot;
	int total_len;
};
#endif
