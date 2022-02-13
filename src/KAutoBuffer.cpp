#include "KAutoBuffer.h"
int KAutoBuffer::write(KHttpRequest *rq, const char *buf, int len)
{
	int wlen;
	char *t = getWriteBuffer(wlen);
	assert(t);
	wlen = MIN(len, wlen);
	kgl_memcpy(t, buf, wlen);		
	writeSuccess(wlen);
	return wlen;
}
void KAutoBuffer::writeSuccess(int got)
{
	assert(last != NULL);
	last->used += got;
	total_len += got;
	hot += got;
}
kbuf *KAutoBuffer::stealBuff()
{
	if (last==NULL || NBUFF_SIZE - last->used < 64) {
		return stealBuffFast();
	}
	kassert(pool == NULL);
	if (last->used > 0) {
		char *nb = (char *)xmalloc(last->used);
		kgl_memcpy(nb, last->data, last->used);
		xfree(last->data);
		last->data = nb;
	}
	return stealBuffFast();
}
char *KAutoBuffer::getWriteBuffer(int &len)
{
	if (last == NULL) {
		assert(head == NULL);
		head = new_buff(NBUFF_SIZE);
		last = head;
		hot = head->data;
	}
	len = NBUFF_SIZE - last->used;
	if (len == 0 || hot==NULL) {
		kbuf *nbuf = new_buff(NBUFF_SIZE);
		assert(last->next == NULL);
		last->next = nbuf;
		last = nbuf;
		hot = last->data;
		len = NBUFF_SIZE;
	}
	assert(len > 0);
	return hot;
}
int KAutoBuffer::getReadBuffer(LPWSABUF buffer, int bufferCount)
{
	if (hot == NULL) {
		return 0;
	}
	assert(head);
	kbuf *tmp = head;
	buffer[0].iov_base = hot;
	buffer[0].iov_len = head->used - (hot - head->data);
	int i;
	for (i = 1; i < bufferCount; i++) {
		tmp = tmp->next;
		if (tmp == NULL) {
			break;
		}
		buffer[i].iov_base = tmp->data;
		buffer[i].iov_len = tmp->used;
	}
	return i;
}
bool KAutoBuffer::readSuccess(int *got)
{
	kassert(hot && head);
	while (*got > 0) {
		int hot_left = head->used - (hot - head->data);
		int this_len = MIN(*got, hot_left);
		hot += this_len;
		*got -= this_len;
		total_len -= this_len;
		if (head->used == hot - head->data) {			
			head = head->next;
			if (head == NULL) {
				last = NULL;
				hot = NULL;
				kassert(total_len == 0);
				return false;
			}
			hot = head->data;
		}
	}
	return true;
}
StreamState KAutoBuffer::write_direct(KHttpRequest *rq, char *data, int len)
{
	kbuf *b = xmemory_new(kbuf);
	memset(b, 0, sizeof(kbuf));
	b->data = data;
	b->used = len;
	Append(b);
	return STREAM_WRITE_SUCCESS;
}
StreamState KAutoBuffer::send(KHttpRequest *rq, KWriteStream *socket)
{
	StreamState result = STREAM_WRITE_SUCCESS;
	kbuf *buf = head;
	while (buf) {
		if (buf->used > 0) {
			result = socket->write_all(rq,buf->data, buf->used);
			if (result == STREAM_WRITE_FAILED) {
				return result;
			}
		}
		buf = buf->next;		
	}
	return STREAM_WRITE_SUCCESS;
}