/*
 * KApiFetchObject.h
 * isapi接口服务类，有两种服务提供方式，看该isapi模块的运行方式。
 * 一种是本地的，即isapi模块挂在kangle主进程上，以多线程的方式运行(服务提供者是KLocalApiService *sa)
 * 另一种是远程的，即isapi模块挂在kangle子进程上，以fastcgi(扩展了)的方式和主进程通信(服务提供者是KFastcgiStream *st)

 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KAPIFETCHOBJECT_H_
#define KAPIFETCHOBJECT_H_
#include "KFetchObject.h"
//#include "KHttpTransfer.h"
#include "KApiRedirect.h"
#include "KApiEnv.h"
#include "KHttpPushParser.h"
#include "KFastcgiUtils.h"
#include "KFetchObject.h"
#include "KApiService.h"

class KApiFetchObject: public KRedirectSource , public KApiService {
public:
	KApiFetchObject(KApiRedirect *rd);
	virtual ~KApiFetchObject();
	KGL_RESULT Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out) override;
	friend class KApiRedirect;
public:
	int writeClient(const char *str, int len) override;
	int readClient(char *buf, int len) override;
	bool setStatusCode(const char *status, int len = 0) override;
	KGL_RESULT addHeader(const char *attr, int len = 0) override;
	KGL_RESULT map_url_path(const char* url, LPVOID file, LPDWORD path_len) override;
	Token_t getToken() override;
	Token_t getVhToken(const char *vh_name);
	Token_t token;
#ifndef _WIN32
	int id[2];
#endif
	KHttpRequest *rq;
	KHttpPushParser push_parser;
private:
	kgl_input_stream* in;
	kgl_output_stream* out;
	kgl_response_body body = { 0 };
	bool initECB(EXTENSION_CONTROL_BLOCK *ecb) override;
};
#endif /* KAPIFETCHOBJECT_H_ */
