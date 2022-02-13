/*
 * KEnvInterface.h
 * 
 * 环境变量接口
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#ifndef KENVINTERFACE_H_
#define KENVINTERFACE_H_
#include <stdlib.h>
#include "kforwin32.h"

class KEnvInterface {
public:
	KEnvInterface();
	virtual ~KEnvInterface();
	virtual bool addEnv(const char *attr, const char *val) = 0;
	virtual bool addEnvEnd() {
		return true;
	}
	virtual const char *getEnv(const char *attr) {
		return NULL;
	}
	virtual bool addContentType(const char *contentType);
	virtual bool addContentLength(const char *contentLength);
public:
	virtual bool addHttpHeader(char *attr, char *val);
	bool addEnv(const char *attr, int val);
protected:
	virtual bool addHttpEnv(const char *attr,const char *val);

};

#endif /* KENVINTERFACE_H_ */
