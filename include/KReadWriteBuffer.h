#ifndef KREADWRITEBUFFER_H
#define KREADWRITEBUFFER_H
#include "KBuffer.h"
#include "KStream.h"
class KReadWriteBuffer: public KWStream
{
public:
	KReadWriteBuffer()
	{
		krw_buffer_init(&buffer, NBUFF_SIZE);	
	}
	KReadWriteBuffer(int chunk_size)
	{
		krw_buffer_init(&buffer, chunk_size);		
	}
	~KReadWriteBuffer()
	{
		krw_buffer_clean(&buffer);
	}
	void destroy()
	{
		krw_buffer_clean(&buffer);
		krw_buffer_init(&buffer, buffer.chunk_size);
	}
	//切换到读模式,返回总大小
	inline unsigned startRead()
	{
		kassert((buffer.head==NULL && buffer.read_hot==NULL && buffer.write_hot==NULL 
			&& buffer.write_hot_buf==NULL && buffer.total_len==0) 
			|| buffer.read_hot == buffer.head->data);
		return (unsigned)buffer.total_len;
	}
	template<typename T>
	inline T *insert()
	{
		kbuf *buf = xmemory_new(kbuf);
		memset(buf, 0, sizeof(kbuf));
		buf->data = (char *)xmalloc(sizeof(T));
		buf->used = sizeof(T);
		krw_insert(&buffer, buf);	
		return (T *)buf->data;
	}
	inline void insertBuffer(kbuf *buf)
	{
		/*
		kassert(buffer.write_hot_buf);
		buf->next = buffer.head;
		buffer.head = buf;
		buffer.total_len += buf->used;
		buffer.read_hot = buffer.head->data;
		*/
		krw_insert(&this->buffer, buf);
	}
	int getReadBuffer(LPWSABUF buffer, int bc)
	{
		return krw_get_read_buffers(&this->buffer, buffer, bc);
	}
	char *getReadBuffer(int &len)
	{
		return krw_get_read_buffer(&buffer, &len);
	}
	bool readSuccess(int got)
	{
		return krw_read_success(&buffer, got);
	}
	void writeSuccess(int got)
	{
		krw_write_success(&buffer, got);
	}
	char *getWriteBuffer(int *len)
	{
		return krw_get_write_buffer(&buffer, len);
	}
	inline void Append(kbuf *buf)
	{
		krw_append(&buffer, buf);
	}
	void write_byte(int ch)
	{
		char temp[2];
		temp[0] = ch;
		write_all(temp,1);
	}
	
	inline void print()
	{
		kbuf *tmp = buffer.head;
		while(tmp){
			if(tmp->used>0){
				fwrite(tmp->data,1,tmp->used,stdout);
			}
			tmp = tmp->next;
		}
	}
	unsigned getLen()
	{
		return buffer.total_len;
	}
	kbuf *getHead()
	{
		return buffer.head;
	}
	kbuf *stealBuff()
	{
		kbuf *ret = buffer.head;
		krw_buffer_init(&buffer, buffer.chunk_size);
		return ret;
	}
	void insert(const char *str, int len) {
		kbuf *buf = (kbuf *)malloc(sizeof(kbuf));
		buf->flags = 0;
		buf->data = (char *)malloc(len);
		kgl_memcpy(buf->data, str, len);
		buf->used = len;
		insertBuffer(buf);
	}
	krw_buffer buffer;
protected:
	int write(const char *buf, int len) override
	{
		int wlen;
		char *t = getWriteBuffer(&wlen);
		assert(t);
		wlen = MIN(len, wlen);
		kgl_memcpy(t, buf, wlen);
		writeSuccess(wlen);
		return wlen;
	}
};
#endif
