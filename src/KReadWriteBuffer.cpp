#include "KReadWriteBuffer.h"
#ifndef _WIN32
#include <sys/uio.h>
#endif
#if 0
int KReadWriteBuffer::getReadBuffer(LPWSABUF buffer,int bufferCount)
{
	if (read_hot==NULL) {
		return 0;
	}
	assert(head);
	kbuf *tmp = head;
	buffer[0].iov_base = read_hot;
	buffer[0].iov_len = head->used - (read_hot - head->data);
	int i;
	for (i=1;i<bufferCount;i++) {
		tmp = tmp->next;
		if (tmp==NULL) {
			break;
		}
		buffer[i].iov_base = tmp->data;
		buffer[i].iov_len = tmp->used;
	}
	return i;
}
void KReadWriteBuffer::writeSuccess(int got)
{
	assert(write_hot_buf!=NULL);
	write_hot_buf->used += got;
	totalLen += got;
	write_hot += got;
}
char *KReadWriteBuffer::getWriteBuffer(int &len)
{
	if (write_hot_buf==NULL) {
		assert(head==NULL);
		head = newbuff();
		read_hot = head->data;
		write_hot_buf = head;
		write_hot = head->data;
	}
	len = chunk_size - write_hot_buf->used;
	if (len == 0) {
		kbuf *nbuf = newbuff();
		assert(write_hot_buf->next==NULL);
		write_hot_buf->next = nbuf;
		write_hot_buf = nbuf;
		write_hot = write_hot_buf->data;
		len = chunk_size;
	}
	assert(len>0);
	return write_hot;
	//*data = hot;
}
char *KReadWriteBuffer::getReadBuffer(int &len)
{
	WSABUF buffer;
	int bufferCount = getReadBuffer(&buffer,1);
	if (bufferCount==0) {
		return NULL;
	}
	len = (int)buffer.iov_len;
	return (char *)buffer.iov_base;
}
bool KReadWriteBuffer::readSuccess(int got)
{
	assert(read_hot && head);
	while (got>0) {
		int hot_left = head->used - (read_hot - head->data);
		int this_len = MIN(got,hot_left);
		read_hot += this_len;
		got -= this_len;
		totalLen -= this_len;
		if (head->used == read_hot - head->data) {
			kbuf *next = head->next;
			xfree(head->data);
			xfree(head);
			head = next;
			if (head==NULL) {
				assert(totalLen == 0);
				init();
				return false;
			}
			read_hot = head->data;
		}
	}
	return true;
}
int KReadWriteBuffer::write(const char *buf, int len)
{	
	int wlen;
	char *t = getWriteBuffer(wlen);
	assert(t);
	wlen = MIN(len,wlen);
	kgl_memcpy(t,buf,wlen);
	writeSuccess(wlen);	
	return wlen;
}
int KReadWriteBuffer::read(char *buf,int len)
{
	char *hot = buf;
	int got = 0;
	while (len>0) {
		int length;
		char *read_data = getReadBuffer(length);
		if (read_data==NULL) {
			return 0;
		}
		length = MIN(length,len);
		if (length<=0) {
			break;
		}
		kgl_memcpy(hot,read_data,length);
		hot += length;
		len -= length;
		got += length;
		if (!readSuccess(length)) {
			break;
		}
	}
	return got;
}
#endif