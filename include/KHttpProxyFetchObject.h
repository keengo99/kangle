/*
 * KHttpProxyFetchObject.h
 *
 *  Created on: 2010-4-20
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

#ifndef KHTTPPROXYFETCHOBJECT_H_
#define KHTTPPROXYFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KAcserver.h"
#include "ksocket.h"
#include "KAsyncFetchObject.h"
#include "KHttpResponseParser.h"
#include "KHttpEnv.h"
class KHttpProxyFetchObject : public KAsyncFetchObject
{
public:
	KHttpProxyFetchObject()
	{
	}
	virtual ~KHttpProxyFetchObject()
	{
	}
protected:
	bool build_http_header(KHttpRequest* rq);
	KGL_RESULT buildHead(KHttpRequest* rq) override;
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		if (rq->ctx.left_read==0) {
			expectDone(rq);
			return false;
		}
		return true;
	}
private:
};

#endif /* KHTTPPROXYFETCHOBJECT_H_ */
