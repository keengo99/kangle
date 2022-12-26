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
	//解析body
	KGL_RESULT ParseBody(KHttpRequest* rq, char** data, char* end) override;
	void BuildPostEnd();
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		return !end_request_recved;
	}
private:
	void ReadBodyEnd(KHttpRequest* rq) {
		end_request_recved = true;
		expectDone(rq);
	}
	kgl_parse_result parse(char** str, char *end, KAjpMessageParser* msg);
	unsigned char parseMessage(KHttpRequest* rq, KHttpObject* obj, KAjpMessageParser* msg);
	int body_len;
	bool end_request_recved;
};

#endif /* KAJPFETCHOBJECT_H_ */
