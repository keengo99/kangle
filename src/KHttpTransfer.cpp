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
#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
#include "cache.h"
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
