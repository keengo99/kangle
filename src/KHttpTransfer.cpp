/*
 * KHttpTransfer.cpp
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

#include "http.h"
#include "KHttpTransfer.h"
#include "kmalloc.h"
#include "KCacheStream.h"
#include "KGzip.h"
#include "KContentTransfer.h"
#include "kselector.h"
#include "KFilterContext.h"
#include "KDefaultHttpStream.h"
#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
#include "cache.h"
#if 0
KHttpTransfer::KHttpTransfer(KHttpRequest *rq, KHttpObject *obj) {
	init(rq, obj);
}
KHttpTransfer::KHttpTransfer() {
	init(NULL, NULL);
}
void KHttpTransfer::init(KHttpRequest *rq, KHttpObject *obj) {
	//deprecated
	assert(false);
	this->obj = obj;
}
KHttpTransfer::~KHttpTransfer() {
	
}
bool KHttpTransfer::support_sendfile(void* arg) {
	auto rq = (KHttpRequest*)arg;
	if (st == NULL) {
		if (sendHead(rq, false) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return false;
		}
	}
	return KHttpStream::forward_support_sendfile(arg);
}
KGL_RESULT KHttpTransfer::sendfile(void* arg, kfiber_file* fp, int64_t *len) {
	auto rq = (KHttpRequest*)arg;
	if (st == NULL) {
		if (sendHead(rq, false) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	return KHttpStream::sendfile(arg, fp, len);
}
StreamState KHttpTransfer::write_all(void *arg, const char *str, int len) {
	auto rq = (KHttpRequest*)arg;
	if (len<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	if (st==NULL) {
		if (sendHead(rq, false) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	if (st == NULL) {
		return STREAM_WRITE_FAILED;
	}
	return st->write_all(rq, str, len);
}
StreamState KHttpTransfer::write_end(void *arg, KGL_RESULT result) {
	auto rq = (KHttpRequest*)arg;
	if (st == NULL) {
		if (sendHead(rq, true) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
			return KHttpStream::write_end(rq, STREAM_WRITE_FAILED);
		}
	}
	return KHttpStream::write_end(rq, result);
}
StreamState KHttpTransfer::sendHead(KHttpRequest *rq, bool isEnd) {
	INT64 start = 0;
	INT64 send_len = 0;
	StreamState result = STREAM_WRITE_SUCCESS;
	INT64 content_len = (isEnd?0:-1);
	cache_model cache_layer = cache_memory;
	kassert(!KBIT_TEST(rq->sink->data.flags, RQ_HAVE_RANGE) || rq->ctx.obj->data->i.status_code==STATUS_CONTENT_PARTIAL);
	if (KBIT_TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
		content_len = obj->index.content_length;
#ifdef ENABLE_DISK_CACHE
		if (content_len > conf.max_cache_size) {
			if (content_len > conf.max_bigobj_size
				|| !obj_can_disk_cache(rq, obj)) {
				cache_layer = cache_none;
			} else {
				cache_layer = cache_disk;
			}
		}
#else
		cache_layer = cache_none;
#endif
	}
	if (rq->needFilter()) {
		/*
		 检查是否需要加载内容变换层，如果要，则长度未知
		 */
		if (rq->ctx.has_change_length_filter) {
			content_len = -1;
		}
		cache_layer = cache_memory;
	}
	KCompressStream *compress_st = NULL;
	//bool gzip_layer = false;
	if (unlikely(rq->ctx.internal && !rq->ctx.replace)) {
		/* 子请求,内部但不是替换模式 */
	} else {
		compress_st = create_compress_stream(rq, obj, content_len);
		if (compress_st) {
			KBIT_SET(rq->sink->data.flags, RQ_TE_COMPRESS);
			content_len = -1;
			cache_layer = cache_memory;
			//obj->insertHttpHeader(kgl_expand_string("Content-Encoding"), kgl_expand_string("gzip"));
			//obj->uk.url->set_content_encoding(compress_st->GetEncoding());
		}
#ifdef WORK_MODEL_TCP
		//端口映射不发送http头
		if (!KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP))
#endif
		build_obj_header(rq, obj, content_len, start, send_len);
#ifndef NDEBUG
		if (KBIT_TEST(rq->sink->data.flags, RQ_TE_COMPRESS)) {
			assert(compress_st);
		} else {
			assert(compress_st == NULL);
		}
#endif
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	}
#if 0
	else if (KBIT_TEST(rq->ctx.filter_flags,RF_ALWAYS_ONLINE) && status_code_can_cache(obj->data->status_code)) {
		//永久在线模式
#if 0
		if (KBIT_TEST(obj->index.flags, ANSW_NO_CACHE)) {
			//如果是动态网页，并且无Cookie(游客模式)
			if (rq->parser.findHttpHeader(kgl_expand_string("Cookie")) == NULL
				&& rq->parser.findHttpHeader(kgl_expand_string("Cookie2")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie2")) == NULL) {
				KBIT_SET(obj->index.flags, OBJ_MUST_REVALIDATE);
				KBIT_CLR(obj->index.flags, ANSW_NO_CACHE | FLAG_DEAD);
			}
		}
#endif
	}
#endif
	loadStream(rq, compress_st,cache_layer);
	return result;
}
bool KHttpTransfer::loadStream(KHttpRequest *rq, KCompressStream *compress_st, cache_model cache_layer) {
#if 0
	/*
	 从下到上开始串流
	 最下面是限速发送.
	 */
	assert(st==NULL && rq);
	st = new KDefaultHttpStream();
	autoDelete = true;
	//检测是否加载cache层

	if (KBIT_TEST(obj->index.flags,ANSW_NO_CACHE)==0) {
		if (cache_layer != cache_none) {
			KCacheStream *st_cache = new KCacheStream(st, autoDelete);
			if (st_cache) {
				st_cache->init(rq,obj,cache_layer);
				st = st_cache;
				autoDelete = true;
			}
		}
	}

	//检测是否要加载gzip压缩层
	if (compress_st) {
		compress_st->connect(st, autoDelete);
		//KGzipCompress *st_gzip = new KGzipCompress(st, autoDelete);
		//debug("加载gzip压缩层=%p,up=%p\n",st_gzip,st);
		//if (st_gzip) {
		st = compress_st;
		autoDelete = true;
		//} else {
		//	return false;
		//}
	}
	//内容变换层
	if ((!rq->ctx.internal || rq->ctx.replace) && rq->needFilter()) {
	//if (!(KBIT_TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
		//debug("加载内容变换层\n");
		if (rq->of_ctx->head) {
			KContentTransfer *st_content = new KContentTransfer(st, autoDelete);
			if (st_content) {
				st = st_content;
				autoDelete = true;
			} else {
				return false;
			}
		}
		KWriteStream *filter_st_head = rq->of_ctx->getFilterStreamHead();
		if (filter_st_head) {				
			KHttpStream *filter_st_end = rq->of_ctx->getFilterStreamEnd();
			assert(filter_st_end);
			if (filter_st_end) {
				filter_st_end->connect(st,autoDelete);
				//st 由rq->of_ctx管理，所以autoDelete为false
				autoDelete = false;
				st = filter_st_head;
			}
		}		
	}
	//串流完毕
#endif
	assert(false);
	return true;
}
#endif

bool kgl_load_response_body(KHttpRequest* rq, kgl_response_body* body) 	{
	INT64 start = 0;
	INT64 send_len = 0;
	StreamState result = STREAM_WRITE_SUCCESS;
	KHttpObject* obj = rq->ctx.obj;
	cache_model cache_layer = cache_memory;
	INT64 content_len = -1;
	if (KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_LENGTH)) {
		content_len = obj->index.content_length;
#ifdef ENABLE_DISK_CACHE
		if (content_len > conf.max_cache_size) {
			if (content_len > conf.max_bigobj_size
				|| !obj_can_disk_cache(rq, obj)) {
				cache_layer = cache_none;
			} else {
				cache_layer = cache_disk;
			}
		}
#else
		cache_layer = cache_none;
#endif
	}
	if (rq->needFilter() && !obj->in_cache) {
		/*
		 检查是否需要加载内容变换层，如果要，则长度未知
		 */
		if (rq->ctx.has_change_length_filter) {
			content_len = -1;
		}
		cache_layer = cache_memory;
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	}
	if (KBIT_TEST(obj->index.flags, ANSW_NO_CACHE) == 0 && body) {
		if (cache_layer != cache_none) {
			pipe_cache_stream(rq, obj, cache_layer,  body);			
		}
	}
	if (unlikely(rq->ctx.internal && !rq->ctx.replace)) {
		/* 子请求,内部但不是替换模式 */
	} else {
		if (body && pipe_compress_stream(rq, obj, content_len, body)) {
			KBIT_SET(rq->sink->data.flags, RQ_TE_COMPRESS);
			content_len = -1;
			cache_layer = cache_memory;
			//obj->insertHttpHeader(kgl_expand_string("Content-Encoding"), kgl_expand_string("gzip"));
			//obj->uk.url->set_content_encoding(compress_st->GetEncoding());
		}
#ifdef WORK_MODEL_TCP
		//端口映射不发送http头
		if (!KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP))
#endif
			if (!build_obj_header(rq, obj, content_len, start, send_len)) {
				return false;
			}
	}
	
	//内容变换层
	if ((!rq->ctx.internal || rq->ctx.replace) && rq->needFilter() && !obj->in_cache) {
#if 0
			//debug("加载内容变换层\n");
		if (rq->of_ctx->head) {
			KContentTransfer* st_content = new KContentTransfer(st, autoDelete);
			if (st_content) {
				st = st_content;
				autoDelete = true;
			} else {
				return false;
			}
		}
		KWriteStream* filter_st_head = rq->of_ctx->getFilterStreamHead();
		if (filter_st_head) {
			KHttpStream* filter_st_end = rq->of_ctx->getFilterStreamEnd();
			assert(filter_st_end);
			if (filter_st_end) {
				filter_st_end->connect(st, autoDelete);
				//st 由rq->of_ctx管理，所以autoDelete为false
				autoDelete = false;
				st = filter_st_head;
			}
		}
#endif
	}
	return true;
}
