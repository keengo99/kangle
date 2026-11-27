/*
 * KAjpFetchObject.h
 * implement ajp/1.3 protocol
 *  Created on: 2010-7-31
 *      Author: keengo
 */

#ifndef KAJPFETCHOBJECT_H_
#define KAJPFETCHOBJECT_H_
#include "global.h"
#include "KAsyncFetchObject.h"
#include "KAcserver.h"
#include "KFileName.h"
#include "KAjpMessage.h"

class KAjpFetchObject : public KAsyncFetchObject
{
public:
	KAjpFetchObject();
	virtual ~KAjpFetchObject();
protected:
	//创建发送头到buffer中。
	KGL_RESULT buildHead(KHttpRequest* rq) override;
	//解析head
	kgl_parse_result parse_unknow_header(KHttpRequest* rq, char** data, char* end) override;
	//创建post数据到buffer中。
	void buildPost(KHttpRequest* rq) override;
	KGL_RESULT on_post_end() override;
	//解析body
	KGL_RESULT ParseBody(KHttpRequest* rq, char** data, char* end) override;
	void BuildPostEnd();
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		return !end_request_recved;
	}
	//the upstream connection can be reused only after END_RESPONSE is received.
	void expectDone(KHttpRequest* rq) override
	{
		if (end_request_recved) {
			KAsyncFetchObject::expectDone(rq);
		}
	}
	KGL_RESULT read_body_end(KHttpRequest* rq, KGL_RESULT result) override
	{
		if (!end_request_recved && us_buffer.used > 0) {
			KGL_RESULT parsed = KAsyncFetchObject::ParseBody(rq);
			if (parsed != KGL_OK) {
				rq->ctx.upstream_connection_keep_alive = 0;
				return parsed;
			}
		}
		return result;
	}
private:
	bool ReadBodyEnd(KHttpRequest* rq, KAjpMessageParser* msg) {
		unsigned char reuse;
		if (!msg->getByte(&reuse) || reuse > 1) {
			return false;
		}
		end_request_recved = true;
		if (!reuse) {
			rq->ctx.upstream_connection_keep_alive = 0;
		}
		expectDone(rq);
		return true;
	}
	kgl_parse_result parse(char** str, char *end, KAjpMessageParser* msg);
	unsigned char parseMessage(KHttpRequest* rq, KHttpObject* obj, KAjpMessageParser* msg);
	int body_len;
	bool end_request_recved;
	/* the empty body packet only ends an unknown-length body */
	bool post_length_unknown;
};

#endif /* KAJPFETCHOBJECT_H_ */
