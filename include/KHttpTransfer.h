/*
 * KHttpTransfer.h
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

#ifndef KHTTPTRANSFER_H_
#define KHTTPTRANSFER_H_
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KCompressStream.h"
#include "KCacheStream.h"
#include "KPushGate.h"
#include "KFilterContext.h"
bool kgl_load_response_body(KHttpRequest* rq, kgl_response_body* body);
inline bool kgl_load_cache_response_body(KHttpRequest* rq, int64_t* body_size) {
	get_default_response_body(rq, &rq->ctx.body);
	if (rq->needFilter()) {
		int32_t flag = KGL_FILTER_CACHE;
		if (rq->sink->data.range != nullptr) {
			flag |= KGL_FILTER_NOT_CHANGE_LENGTH;
		}
		if (rq->of_ctx->tee_body(rq, &rq->ctx.body, flag)) {
			assert(rq->sink->data.range == nullptr);
			*body_size = -1;
		}
	}
	return true;
}
#endif /* KHTTPTRANSFER_H_ */
