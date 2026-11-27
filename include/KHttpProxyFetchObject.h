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
#include "KPushGate.h"
#include <string>
/* WebSocket handshake translation between HTTP/2 extended CONNECT (RFC 8441) and HTTP/1.1 Upgrade */
#define KGL_WS_TO_H1 1  /* HTTP/2 client, HTTP/1.1 origin */
#define KGL_WS_TO_H2 2  /* HTTP/1.1 client, HTTP/2 origin */
class KHttpProxyFetchObject : public KAsyncFetchObject
{
public:
	KHttpProxyFetchObject()
	{
	}
	virtual ~KHttpProxyFetchObject()
	{
	}
	bool push_upstream_header(KHttpRequest* rq, const char* attr, int attr_len, const char* val, int val_len, bool request_line);
protected:
	bool build_http_header(KHttpRequest* rq);
	KGL_RESULT buildHead(KHttpRequest* rq) override;
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		if (rq->ctx.left_read==0 && !client->IsMultiStream()) {
			expectDone(rq);
			return false;
		}
		return true;
	}
	bool upstream_handle_expect() override
	{
		return true;
	}
	bool upstream_chunk_post() override
	{
		return true;
	}
	KGL_RESULT ParseBody(KHttpRequest* rq, char** data, char* end) override
	{
		KGL_RESULT result = KAsyncFetchObject::ParseBody(rq, data, end);
		//下游写入也可能返回KGL_END(不再需要数据)，必须确认chunked已读到结束块
		if (result == KGL_END && pop_header.upstream_is_chunk && is_dechunk_body_end(&body)) {
			expectDone(rq);
		}
		return result;
	}
private:
	uint8_t ws_translate = 0;
	/* the HTTP/1.1 client's Sec-WebSocket-Key (KGL_WS_TO_H2) */
	std::string ws_key;
};

#endif /* KHTTPPROXYFETCHOBJECT_H_ */
