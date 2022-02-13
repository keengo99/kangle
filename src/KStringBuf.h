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

#ifndef KSTRING_H_
#define KSTRING_H_
#include <string>
#include <stdlib.h>
#include <assert.h>
#include "global.h"
#include "KStream.h"
#include "kmalloc.h"

class KStringBuf : public KWStream {
public:
	KStringBuf(int size);
	KStringBuf();
	virtual ~KStringBuf();
	char *getString() {
		if (buf == NULL) {
			return NULL;
		}
		kassert(hot && buf);
		*hot = '\0';
		return buf;
	}
	void init(int size);
	char *getBuf() {
		return buf;
	}
	int getSize() {
		if (buf == NULL) {
			return 0;
		}
		return (int)(hot - buf);
	}
	char *stealString() {
		*hot = '\0';
		char *str = buf;
		hot = NULL;
		buf = NULL;
		return str;
	}
	void clean()
	{
		hot = buf;
	}
	StreamState write_all(const char *str, int len);
private:
	char *buf;
	char *hot;
	int current_size;
};
class KFixString : public KWStream {
public:
	KFixString(char *buf,int len)
	{
		this->buf = buf;
		this->hot = buf;
		this->left = len;
	};
	int getSize()
	{
		return (int)(hot-buf);
	}
	StreamState write_all(const char *str,int len)
	{
		int send_len = MIN(len,left);
		if(send_len<=0){
			return STREAM_WRITE_FAILED;
		}
		kgl_memcpy(hot,str,send_len);
		hot += send_len;
		left -= send_len;
		return STREAM_WRITE_SUCCESS;
	}
private:
	char *buf;
	char *hot;
	int left;
};
#endif /* KSTRING_H_ */
