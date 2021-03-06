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
#ifndef KGZIP_H_
#define KGZIP_H_
#include "zlib.h"
#include "ksocket.h"
//#include "KBuffer.h"
//#include "KHttpRequest.h"
//#include "KHttpObject.h"
#include "KCompressStream.h"

class KHttpTransfer;
enum {
	GZIP_DECOMPRESS_ERROR, GZIP_DECOMPRESS_CONTINUE, GZIP_DECOMPRESS_END
};
class KGzipDecompress : public KHttpStream
{
public:
	KGzipDecompress(bool use_deflate,KWriteStream *st,bool autoDelete);
	~KGzipDecompress();
	KGL_RESULT write_all(void *rq, const char *str,int len) override;
	KGL_RESULT write_end(void*rq, KGL_RESULT result) override;
private:
	KGL_RESULT decompress(void*rq, int flush_flag);
	z_stream strm;
	bool isSuccess;
	bool use_deflate;
	int in_skip ;
};
class KGzipCompress : public KCompressStream
{
public:
	KGzipCompress(int gzip_level);
	~KGzipCompress();
	KGL_RESULT write_all(void*rq, const char *str,int len) override;
	KGL_RESULT write_end(void*rq, KGL_RESULT result) override;
	void setFast(bool fast)
	{
		this->fast = fast;
	}
	KGL_RESULT flush(void*rq) override
	{
		return compress(rq, Z_SYNC_FLUSH);
	}
private:
	KGL_RESULT compress(void *rq, int flush_flag);
	z_stream strm;
	char *out;
	unsigned used;
	uLong crc;
	bool fast;
	bool isSuccess;
};
#endif /* KGZIP_H_ */
