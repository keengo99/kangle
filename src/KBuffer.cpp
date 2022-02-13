/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include <string.h>
#include "kmalloc.h"
#include "KBuffer.h"
StreamState send_buff(KSendable *socket, kbuf *send_buffer , INT64 &start,INT64 &send_len)
{
	INT64 this_send_len;
	StreamState result = STREAM_WRITE_SUCCESS;
	while (send_buffer) {
		if ((INT64) send_buffer->used <= start) {
			start -= send_buffer->used;
			goto next_buffer;
		}
		//if (!send_unknow_length) {
		if (send_len <= 0){
				break;
		}
		if (send_len > (INT64) send_buffer->used - start) {
			this_send_len = send_buffer->used - (unsigned) start;
		} else {
			this_send_len = (int) send_len;
		}
		result = socket->write_all(send_buffer->data + start, (int)this_send_len);
		if (result == STREAM_WRITE_FAILED) {
			break;
		}
		start = 0;
		send_len -= this_send_len;
		next_buffer: send_buffer = send_buffer->next;
	}
	return result;
}
#if 0
KBuffer::KBuffer() {
	init(CHUNK);
}
KBuffer::KBuffer(int chunkSize) {
	init(chunkSize);
}
KBuffer::~KBuffer() {
	clean();
}
void KBuffer::clean() {
	if (hotData != NULL) {
		xfree(hotData);
		hotData = NULL;
	}
	if (out_buf != NULL) {
		destroy_kbuf(out_buf);
		out_buf = NULL;
	}
	totalLen = 0;
	used = 0;
	hot_buf = NULL;
}

void KBuffer::init(int chunkSize) {
	this->chunkSize = chunkSize;
	hotData = NULL;//(char *)malloc(chunkSize);
	used = 0;
	totalLen = 0;
	out_buf = NULL;
	hot_buf = NULL;
}
/*
 kbuf *KBuffer::getHotBuf() {
 return hot_buf;
 }
 */
StreamState KBuffer::send(KSendable *socket,INT64 start,INT64 len)
{
	StreamState result = STREAM_WRITE_SUCCESS;
	if(out_buf){
		result = send_buff(socket,out_buf,start,len);
		if(result == STREAM_WRITE_FAILED){
			return result;
		}
	}
	assert(start >= 0);
	if(hotData && used>0){
		if(start + len > used){
			return STREAM_WRITE_FAILED;
		}
		return socket->write_all(hotData + (int)start,(int)len);
	}
	return result;
}
StreamState KBuffer::send(KSendable *socket) {
	StreamState result = STREAM_WRITE_SUCCESS;
	if (out_buf) {
		result = send_buff<KSendable>(socket, out_buf);
		if(result == STREAM_WRITE_FAILED) {
			return result;
		}
	}
	if (hotData) {
		return socket->write_all(hotData, used);
	}
	return result;
}
void KBuffer::add(const char *buf) {
	write_all(buf, strlen(buf));
}
StreamState KBuffer::write_all(const char *buf, int len) {
	if (len <= 0) {
		return STREAM_WRITE_SUCCESS;
	}
	totalLen += len;
	for (;;) {
		if (hotData == NULL) {
			assert(used==0);
			hotData = (char *) xmalloc(chunkSize);
			if (hotData == NULL) {
				return STREAM_WRITE_FAILED;
			}
		}
		int maxSave = chunkSize - used;
		if (maxSave > 0) {
			int copyLen = maxSave < len ? maxSave : len;
			if (copyLen <= 0)
				break;
			kgl_memcpy(hotData + used, buf, copyLen);
			used += copyLen;
			len -= copyLen;
			buf += copyLen;
		}
		if (used >= chunkSize) {
			internelAdd(hotData, used);
			used = 0;
			hotData = NULL;
		}
	}
	return STREAM_WRITE_SUCCESS;
}
void KBuffer::internelEnd(char *buf, int len, int addBytes) {
	if (len + addBytes > 0) {
		if (hot_buf == NULL) {
			hot_buf = (kbuf *) xmalloc(sizeof(kbuf));
			out_buf = hot_buf;
		} else {
			hot_buf->next = (kbuf *) xmalloc(sizeof(kbuf));
			hot_buf = hot_buf->next;
		}
		//      printf("addBytes=%d\n",addBytes);
		hot_buf->data = (char *) xmalloc(len + addBytes);
		hot_buf->next = NULL;
		hot_buf->flags = 0;
		if (len > 0) {
			kgl_memcpy(hot_buf->data, buf, len);
		}
		hot_buf->used = len;
	}
}
void KBuffer::end(char *buf, int len, int addBytes) {
	totalLen += len;
	internelEnd(buf, len, addBytes);
}
kbuf *KBuffer::getAllBuf()
{
	if (hotData == NULL) {
		return out_buf;
	}
	if (hot_buf == NULL) {
		hot_buf = (kbuf *) malloc(sizeof(kbuf));
		out_buf = hot_buf;
	} else {
		hot_buf->next = (kbuf *) malloc(sizeof(kbuf));
		hot_buf = hot_buf->next;
	}
	hot_buf->data = hotData;
	hot_buf->next = NULL;
	hot_buf->used = used;
	hot_buf->flags = 0;
	used = 0;
	hotData = NULL;
	return out_buf;
}
kbuf *KBuffer::stealBuffFast() {
	kbuf *ret_buf;
	if (hotData == NULL) {
		ret_buf = out_buf;
		hot_buf = NULL;
		out_buf = NULL;
		return ret_buf;
	}
	if (hot_buf == NULL) {
		hot_buf = (kbuf *) malloc(sizeof(kbuf));
		assert(out_buf == NULL);
		out_buf = hot_buf;
	} else {
		assert(out_buf != NULL);
		hot_buf->next = (kbuf *) malloc(sizeof(kbuf));
		hot_buf = hot_buf->next;
	}
	hot_buf->data = hotData;
	hot_buf->next = NULL;
	hot_buf->used = used;
	hot_buf->flags = 0;
	ret_buf = out_buf;
	out_buf = NULL;
	hotData = NULL;
	used = 0;
	totalLen = 0;
	hot_buf = NULL;
	return ret_buf;
}
StreamState KBuffer::write_direct(char *buf, int len) {
	totalLen += len;
	internelAdd(buf, len);
	return STREAM_WRITE_SUCCESS;
}
void KBuffer::internelAdd(char *buf, int len) {
	if (hot_buf == NULL) {
		assert(out_buf == NULL);
		hot_buf = (kbuf *) xmalloc(sizeof(kbuf));
		out_buf = hot_buf;
	} else {
		assert(out_buf != NULL);
		hot_buf->next = (kbuf *) xmalloc(sizeof(kbuf));
		hot_buf = hot_buf->next;
	}
	hot_buf->flags = 0;
	hot_buf->data = buf;
	hot_buf->next = NULL;
	hot_buf->used = len;
}
kbuf *KBuffer::stealBuff() {
	if (chunkSize - used < 64) {
		return stealBuffFast();
	}
	if (hotData) {
		assert(used>=0);
		internelEnd(hotData, used);
		xfree(hotData);
		hotData = NULL;
		used = 0;
	}
	kbuf *result = out_buf;
	out_buf = NULL;
	hot_buf = NULL;
	totalLen = 0;
	return result;
}
unsigned KBuffer::getLen() {
	//printf("getLen=%d\n",totalLen);
	return totalLen;
}

void KBuffer::dump(FILE *fp)
{
	kbuf *t = out_buf;
	while(t){
		fwrite(t->data,1,t->used,fp);
		t = t->next;
	}
	if(hotData){
		fwrite(hotData,1,used,fp);
	}
}
#endif