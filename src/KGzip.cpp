/*
 * KGzip.cpp
 *
 *  Created on: 2010-5-4
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#include <string.h>
 //#include "do_config.h"
#include "KGzip.h"
//#include "KHttpTransfer.h"
#include "kmalloc.h"
#include "klog.h"
#define GZIP_CHUNK 8192
KGzipCompress::KGzipCompress(int gzip_level) : KCompressStream(NULL) {
	fast = false;
	isSuccess = false;
	//this->use_deflate = use_deflate;
	memset(&strm, 0, sizeof(strm));
	if (deflateInit2(&strm, gzip_level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
		return;
	}
	out = (char*)malloc(GZIP_CHUNK);
	if (out == NULL) {
		klog(KLOG_ERR, "no memory to alloc\n");
		return;
	}
	//if (!use_deflate) {
	sprintf(out, "%c%c%c%c%c%c%c%c%c%c", 0x1f, 0x8b, Z_DEFLATED, 0 /*flags*/,
		0, 0, 0, 0 /*time*/, 0 /*xflags*/, 0);
	used = 10;
	crc = crc32(0L, Z_NULL, 0);
	//}
	isSuccess = true;
}
KGzipCompress::~KGzipCompress() {
	if (out) {
		xfree(out);
	}
	deflateEnd(&strm);
}
StreamState KGzipCompress::write_all(void* rq, const char* str, int len) {
	strm.avail_in = len;
	strm.next_in = (unsigned char*)str;
	//if (!use_deflate) {
	crc = crc32(crc, (unsigned char*)str, len);
	//}
	return compress(rq, Z_NO_FLUSH);
}
StreamState KGzipCompress::write_end(void* rq, KGL_RESULT result) {
	if (strm.total_in <= 0) {
		return KHttpStream::write_end(rq, STREAM_WRITE_FAILED);
	}
	KGL_RESULT result2 = compress(rq, Z_FINISH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	if (used + 8 > GZIP_CHUNK || !fast) {
		char* buf = (char*)xmalloc(used + 8);
		kgl_memcpy(buf, out, used);
		xfree(out);
		out = buf;
	}
	//if (!use_deflate) {
	int n;
	for (n = 0; n < 4; n++) {
		out[used + n] = (crc & 0xff);
		crc >>= 8;
	}
	used += 4;
	unsigned totalLen2 = strm.total_in;
	for (n = 0; n < 4; n++) {
		out[used + n] = (totalLen2 & 0xff);
		totalLen2 >>= 8;
	}
	used += 4;
	//}
	result2 = st->write_all(rq, out, used);
	xfree(out);
	out = NULL;
	if (result2 != STREAM_WRITE_SUCCESS) {
		return KHttpStream::write_end(rq, result2);
	}
	return KHttpStream::write_end(rq, result);
}
StreamState KGzipCompress::compress(void* rq, int flush_flag) {
	if (!isSuccess) {
		return STREAM_WRITE_FAILED;
	}
	do {
		if (out == NULL) {
			out = (char*)xmalloc(GZIP_CHUNK);
			if (out == NULL) {
				klog(KLOG_ERR, "no memory to alloc\n");
				return STREAM_WRITE_FAILED;
			}
			used = 0;
		}
		strm.avail_out = GZIP_CHUNK - used;
		strm.next_out = (unsigned char*)(out + used);
		int ret = deflate(&strm, flush_flag);//(flush_flag ? Z_FINISH : Z_NO_FLUSH)); /* no bad return value */
		//		int ret = deflate(&strm, (flush ? Z_FINISH : Z_SYNC_FLUSH)); /* no bad return value */

		assert(ret != Z_STREAM_ERROR); /* state not clobbered */
		if (ret == Z_STREAM_ERROR) {
			return STREAM_WRITE_FAILED;
		}
		unsigned have = GZIP_CHUNK - used - strm.avail_out;
		used += have;
		if (used >= GZIP_CHUNK) {
			StreamState result = st->write_all(rq, out, used);
			free(out);
			//printf("this=%p,up=%p\n",this,st);
			out = NULL;
			if (result != STREAM_WRITE_SUCCESS) {
				return result;
			}
		}
	} while (strm.avail_out == 0);
	return STREAM_WRITE_SUCCESS;
}
KGzipDecompress::KGzipDecompress(bool use_deflate, KWriteStream* st, bool autoDelete) : KHttpStream(st, autoDelete) {
	isSuccess = false;
	memset(&strm, 0, sizeof(strm));
	this->use_deflate = use_deflate;
	if (use_deflate) {
		in_skip = 0;
	} else {
		in_skip = 10;
	}
	if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
		return;
	}
	isSuccess = true;
	return;
}
StreamState KGzipDecompress::decompress(void* rq, int flush_flag) {
	int ret = 0;
	char out[GZIP_CHUNK];
	do {
		strm.avail_out = GZIP_CHUNK;
		strm.next_out = (unsigned char*)out;
		ret = inflate(&strm, flush_flag); /* no bad return value */
		switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			return STREAM_WRITE_FAILED;
		case Z_BUF_ERROR:
			break;
		}
		int have = GZIP_CHUNK - strm.avail_out;
		if (have > 0) {
			StreamState result = st->write_all(rq, out, have);
			if (result != STREAM_WRITE_SUCCESS) {
				return result;
			}
		}
	} while (strm.avail_out == 0);
	return STREAM_WRITE_SUCCESS;
}
StreamState KGzipDecompress::write_end(void* rq, KGL_RESULT result) {
	KGL_RESULT result2 = decompress(rq, Z_FINISH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(rq, result2);
	}
	return KHttpStream::write_end(rq, result);
}
StreamState KGzipDecompress::write_all(void* rq, const char* str, int len) {
	int skip_len = KGL_MIN(in_skip, len);
	in_skip -= skip_len;
	strm.avail_in = len - skip_len;
	strm.next_in = (unsigned char*)str + skip_len;
	if (strm.avail_in > 0) {
		return decompress(rq, Z_NO_FLUSH);
	}
	return STREAM_WRITE_SUCCESS;
}
KGzipDecompress::~KGzipDecompress() {
	inflateEnd(&strm);
}
