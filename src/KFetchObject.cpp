/*
 * KFetchObject.cpp
 *
 *  Created on: 2010-4-19
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
#include "KFetchObject.h"
#include "kmalloc.h"
#include "KAsyncFetchObject.h"
#include "KHttpTransfer.h"
#include "HttpFiber.h"

KFetchObject::~KFetchObject()
{
	if (brd) {
		brd->release();
	}
}

bool KFetchObject::NeedTempFile(bool upload, KHttpRequest *rq)
{
	if (!upload) {
		return true;
	}
	return rq->content_length == -1;
}
uint32_t KFetchObject::Check(KHttpRequest* rq)
{
	return CheckResult(rq, KF_STATUS_REQ_TRUE);
}
uint32_t KFetchObject::CheckResult(KHttpRequest* rq, uint32_t result)
{
	KFetchObject* next = NULL;
	if (TEST(result, KF_STATUS_REQ_TRUE|KF_STATUS_REQ_FINISHED) == KF_STATUS_REQ_TRUE) {
		next = rq->GetNextFetchObject(this);
	}
	if (TEST(result, KF_STATUS_REQ_SKIP_OPEN)) {
		klist_remove(&this->queue);
		delete this;
	}
	if (next) {
		return next->Check(rq);
	}
	return result;
}
KGL_RESULT KFetchObject::PushBody(KHttpRequest *rq, kgl_output_stream *out, const char *buf, int len)
{
	if (!rq->ctx->connection_upgrade && rq->ctx->know_length) {
		len = (int)MIN(rq->ctx->left_read, (INT64)len);
		rq->ctx->left_read -= len;
	}
	return out->f->write_body(out, rq, buf, len);
}
uint32_t KRefFetchObject::Check(KHttpRequest *rq)
{
	assert(rq->GetNextFetchObject(this) == NULL);
	klist_remove(&queue);
	if (this->fo) {
		rq->AppendFetchObject(this->fo);
		this->fo = NULL;
	}
	delete this;
	return KF_STATUS_REQ_FALSE;
	/*
	KGL_RESULT ret = fiber_http_start(rq);
	delete this;
	return ret;
	*/
}
