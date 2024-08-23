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
#include "kselector.h"
#include "KFilterContext.h"
#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
#include "cache.h"
bool kgl_load_response_body(KHttpRequest* rq, kgl_response_body* body) {
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
		cache_layer = cache_memory;
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	}

	if (body) {
#ifdef ENABLE_TF_EXCHANGE
		if (rq->NeedTempFile(false)) {
			if (!tee_tempfile_body(body)) {
				klog(KLOG_ERR, "cann't tee tempfile output body\n");
			}
		}
#endif
		//ÄÚÈİ±ä»»²ã
		if (rq->needFilter() && obj->in_cache) {
			if (rq->of_ctx->tee_body(rq, body, KGL_FILTER_CACHE)) {
				content_len = -1;
			}
		}
		if (KBIT_TEST(obj->index.flags, ANSW_NO_CACHE) == 0) {
			if (cache_layer != cache_none) {
				pipe_cache_stream(rq, obj, cache_layer, body);
			}
		}
		if (pipe_compress_stream(rq, obj, content_len, body)) {
			KBIT_SET(rq->sink->data.flags, RQ_TE_COMPRESS);
			content_len = -1;
			cache_layer = cache_memory;
		}
		//ÄÚÈİ±ä»»²ã
		if (rq->needFilter() && !obj->in_cache) {
			if (rq->of_ctx->tee_body(rq, body, KGL_FILTER_NOT_CACHE)) {
				content_len = -1;
			}
		}
	}
	if (!rq->ctx.no_http_header) {
		return build_obj_header(rq, obj, content_len);
	}
	return true;
}
