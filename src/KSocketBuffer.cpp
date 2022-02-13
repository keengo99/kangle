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
#include "KSocketBuffer.h"
#ifndef _WIN32
#include <sys/uio.h>
#endif
#if 0
int KSocketBuffer::getReadBuffer(LPWSABUF buffer,int bufferCount)
{
	if(hot==NULL){
		return 0;
	}
	assert(hot_buf);
	kbuf *tmp = hot_buf;
	buffer[0].iov_base = hot;
	buffer[0].iov_len = hot_buf->used - (hot - hot_buf->data);
	int i;
	for(i=1;i<bufferCount;i++){
		tmp = tmp->next;
		if (tmp==NULL) {
			break;
		}
		buffer[i].iov_base = tmp->data;
		buffer[i].iov_len = tmp->used;
	}
	return i;
}
void KSocketBuffer::writeSuccess(int got)
{
	assert(hot_buf!=NULL);
	hot_buf->used += got;
	totalLen += got;
	hot += got;
}
char *KSocketBuffer::getWriteBuffer(int &len)
{
	if(hot_buf==NULL){
		assert(head==NULL);
		head = newbuff();
		hot_buf = head;
		hot = hot_buf->data;
	}
	len = chunk_size - hot_buf->used;
	if (len == 0) {
		kbuf *nbuf = newbuff();
		assert(hot_buf->next==NULL);
		hot_buf->next = nbuf;
		hot_buf = nbuf;
		hot = hot_buf->data;
		len = chunk_size;
	}
	assert(len>0);
	return hot;
	//*data = hot;
}
char *KSocketBuffer::getReadBuffer(int &len)
{
	if (hot_buf==NULL) {
		return NULL;
	}
	assert(hot);
	len = (hot_buf->used - (hot - hot_buf->data));
	//printf("hot_buf->used=%d,hot = %p\n",hot_buf->used,hot);
	if (len==0) {
		hot_buf = hot_buf->next;
		if(hot_buf==NULL){			
			return NULL;
		}
		len = hot_buf->used;
		hot = hot_buf->data;
	}
	return hot;
}
bool KSocketBuffer::readSuccess(int got)
{
	assert(hot && hot_buf);
	while (got>0) {
		int hot_left = hot_buf->used - (hot - hot_buf->data);
		int this_len = MIN(got,hot_left);
		hot += this_len;
		got -= this_len;
		if(hot_buf->used == hot-hot_buf->data){
			hot_buf = hot_buf->next;
			if(hot_buf==NULL){
				return false;
			}
			hot = hot_buf->data;
		}
	}
	return true;
}
StreamState KSocketBuffer::write_all(const char *buf, int len)
{	
	while (len>0) {
		int wlen;
		char *t = getWriteBuffer(wlen);
		assert(t);
		wlen = MIN(len,wlen);
		kgl_memcpy(t,buf,wlen);
		buf += wlen;
		len -= wlen;
		writeSuccess(wlen);
	}
	return STREAM_WRITE_SUCCESS;
}
int KSocketBuffer::read(char *buf,int len)
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