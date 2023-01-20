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
#include "KGzip.h"
#include "kmalloc.h"
#include "klog.h"
#include "KPushGate.h"
#define GZIP_CHUNK 8192
#if 0
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
#endif
KGzipDecompress::KGzipDecompress(bool use_deflate, KWStream* st, bool autoDelete) : KHttpStream(st) {
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
StreamState KGzipDecompress::decompress( int flush_flag) {
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
			StreamState result = st->write_all( out, have);
			if (result != STREAM_WRITE_SUCCESS) {
				return result;
			}
		}
	} while (strm.avail_out == 0);
	return STREAM_WRITE_SUCCESS;
}
StreamState KGzipDecompress::write_end(KGL_RESULT result) {
	KGL_RESULT result2 = decompress(Z_FINISH);
	if (result2 != KGL_OK) {
		return KHttpStream::write_end(result2);
	}
	return KHttpStream::write_end( result);
}
StreamState KGzipDecompress::write_all(const char* str, int len) {
	int skip_len = KGL_MIN(in_skip, len);
	in_skip -= skip_len;
	strm.avail_in = len - skip_len;
	strm.next_in = (unsigned char*)str + skip_len;
	if (strm.avail_in > 0) {
		return decompress(Z_NO_FLUSH);
	}
	return STREAM_WRITE_SUCCESS;
}
KGzipDecompress::~KGzipDecompress() {
	inflateEnd(&strm);
}
struct kgzip_context : kgl_forward_body
{
	z_stream strm;
	uLong crc;
	bool data_has_write;
};

StreamState kgzip_compress(kgzip_context *ctx, int flush_flag) {
	char out[GZIP_CHUNK];
	int used = 0;
	if (!ctx->data_has_write) {
		used = sprintf(out, "%c%c%c%c%c%c%c%c%c%c", 0x1f, 0x8b, Z_DEFLATED, 0 /*flags*/,
			0, 0, 0, 0 /*time*/, 0 /*xflags*/, 0);
		ctx->crc = crc32(0L, Z_NULL, 0);
		ctx->data_has_write = true;
	}
	do {
		ctx->strm.avail_out = GZIP_CHUNK - used;
		ctx->strm.next_out = (unsigned char*)(out + used);
		int ret = deflate(&ctx->strm, flush_flag);
		assert(ret != Z_STREAM_ERROR); /* state not clobbered */
		if (ret == Z_STREAM_ERROR) {
			return STREAM_WRITE_FAILED;
		}
		unsigned have = GZIP_CHUNK -ctx->strm.avail_out;
		if (have > 0) {
			KGL_RESULT result = forward_write((kgl_response_body_ctx*)ctx, out, have);
			if (result != STREAM_WRITE_SUCCESS) {
				return result;
			}
		}
		used = 0;
	} while (ctx->strm.avail_out == 0);
	return STREAM_WRITE_SUCCESS;
}
static KGL_RESULT kgzip_write(kgl_response_body_ctx* ctx, const char* buf, int size) {
	kgzip_context* gzip = (kgzip_context*)ctx;
	gzip->strm.avail_in = size;
	gzip->strm.next_in = (unsigned char*)buf;
	KGL_RESULT result = kgzip_compress(gzip, Z_NO_FLUSH);
	if (result == KGL_OK) {
		gzip->crc = crc32(gzip->crc, (unsigned char*)buf, size);
	}
	return result;
}
KGL_RESULT kgzip_flush(kgl_response_body_ctx* ctx) {
	kgzip_context* gzip = (kgzip_context*)ctx;
	return kgzip_compress(gzip, Z_SYNC_FLUSH);
}
KGL_RESULT kgzip_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	kgzip_context* gzip = (kgzip_context*)ctx;
	if (result != KGL_OK) {
		goto done;
	}
	if (gzip->strm.total_in <= 0) {
		result = KGL_EDATA_FORMAT;
		goto done;
	}
	result = kgzip_compress(gzip, Z_FINISH);
	if (result != KGL_OK) {
		goto done;
	}
	u_char out[8];
	int n;
	for (n = 0; n < 4; n++) {
		out[n] = (gzip->crc & 0xff);
		gzip->crc >>= 8;
	}
	unsigned total_length = gzip->strm.total_in;
	for (n = 0; n < 4; n++) {
		out[n+4] = (total_length & 0xff);
		total_length >>= 8;
	}
	result = forward_write(ctx, (char *)out, 8);
done:
	result = forward_close(ctx, result);
	deflateEnd(&gzip->strm);
	delete gzip;
	return result;
}
static _kgl_response_body_function kgzip_function = {
	unsupport_writev<kgzip_write>,
	kgzip_write,
	kgzip_flush,
	support_sendfile_false,
	unsupport_sendfile,
	kgzip_close,
};
bool pipe_gzip_compress(int gzip_level, kgl_response_body* body) {
	kgzip_context* gzip = new kgzip_context;
	memset(gzip, 0, sizeof(kgzip_context));
	if (deflateInit2(&gzip->strm, gzip_level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
		delete gzip;
		return false;
	}
	pipe_response_body(gzip, &kgzip_function, body);
	return true;
}