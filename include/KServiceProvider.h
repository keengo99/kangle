/*
 * KServiceProvider.h
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */

#ifndef KSERVICEPROVIDER_H_
#define KSERVICEPROVIDER_H_
//#include "utils.h"
#include "KStream.h"
#include "kforwin32.h"
class KServiceProvider {
public:
	KServiceProvider();
	virtual ~KServiceProvider();
	virtual KWStream *getOutputStream() = 0;
	virtual KRStream *getInputStream() = 0;
	virtual char getMethod() = 0;
	virtual const char *getFileName() = 0;
	virtual const char *getQueryString() = 0;
	virtual const char *getRequestUri() = 0;
	virtual INT64 getContentLength() = 0;
	virtual const char *getContentType() = 0;
	virtual Token_t getToken() = 0;
	virtual bool execUrl(const char *url) = 0;
	virtual void log(const char *str) = 0;
	/*
	return http header val ,
	note: the val must call freeHttpHeader to free.
	*/
	virtual char *getHttpHeader(const char *attr) = 0;
	virtual bool getEnv(const char *attr,char *val,int *len) = 0;
	virtual void freeHttpHeader(char *val)
	{

	}
	virtual bool isHeadSend() = 0;
	virtual bool sendKnowHeader(int attr, const char *val) = 0;
	virtual bool sendUnknowHeader(const char *attr, const char *val) = 0;
	virtual bool sendStatus(int statusCode, const char *statusLine) = 0;
private:

};

#endif /* KSERVICEPROVIDER_H_ */
