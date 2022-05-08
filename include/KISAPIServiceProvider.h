/*
 * KISAPIServiceProvider.h
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */

#ifndef KISAPISERVICEPROVIDER_H_
#define KISAPISERVICEPROVIDER_H_
#include <sstream>
#include <vector>
//#include "KApiFetchObject.h"
#include "KServiceProvider.h"
#include "khttpext.h"
#include "KHttpKeyValue.h"

#include "KStream.h"
#include "KStringBuf.h"

class KISAPIServiceProvider: public KServiceProvider, public KStream {
public:
	KISAPIServiceProvider();
	virtual ~KISAPIServiceProvider();
	Token_t getToken() {
		Token_t token;
		if (pECB->ServerSupportFunction(pECB->ConnID,
				HSE_REQ_GET_IMPERSONATION_TOKEN, (LPVOID) &token, NULL, NULL)
				== TRUE) {
			return token;
		}
		return NULL;
	}
	KWStream *getOutputStream() {
		return this;
	}
	KRStream *getInputStream() {
		return this;
	}
	bool execUrl(const char *url);
	void log(const char *str);
	int64_t get_read_left() override
	{
		return pECB->cbLeft;
	}
	int read(char *buf, int len);
	int write(const char *buf, int len);
	StreamState write_end(KGL_RESULT result) override;
	char getMethod() {
		if (meth != METH_UNSET) {
			return meth;
		}
		meth = KHttpKeyValue::getMethod(pECB->lpszMethod);
		return meth;
	}
	const char *getFileName() {
		return pECB->lpszPathTranslated;
	}
	const char *getQueryString() {
		return pECB->lpszQueryString;
	}
	const char *getRequestUri() {
		return pECB->lpszPathInfo;
	}
	INT64 getContentLength() {
		return pECB->cbTotalBytes;
	}
	const char *getContentType() {
		return pECB->lpszContentType;
	}	
	char *getHttpHeader(const char *attr) {
		std::stringstream s;
		s << "HTTP_";
		while (*attr) {
			if (*attr == '-') {
				s << "_";
			} else {
				s << (char) toupper(*attr);
			}
			attr++;
		}
		int len = sizeof(header_val);
		if (getEnv(s.str().c_str(), header_val, &len)) {
			return header_val;
		}
		return NULL;
	}
	bool isHeadSend() {
		return headSended;
	}
	bool sendKnowHeader(int attr, const char *val) {
		return true;
	}
	bool sendUnknowHeader(const char *attr, const char *val);
	bool sendStatus(int statusCode, const char *statusLine);
	bool getEnv(const char *attr, char *val, int *len);

	//bool getServerVar(const char *name, char *value, int len);
	void setECB(EXTENSION_CONTROL_BLOCK *pECB) {
		this->pECB = pECB;
	}
	EXTENSION_CONTROL_BLOCK* pECB;
private:
	char header_val[512];
	void checkHeaderSend();
	char meth;
	char *docRoot;
	bool headSended;	
	KStringBuf s;
	KStringBuf status;
};

#endif /* KISAPISERVICEPROVIDER_H_ */
