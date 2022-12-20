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
#include "kforwin32.h"
#include <stdlib.h>
#include <string.h>

class KEnvInterface {
public:
	KEnvInterface();
	virtual ~KEnvInterface();
	virtual bool addEnv(const char* attr, const char* val)
	{
		return add_env(attr, strlen(attr), val, strlen(val));
	}
	virtual bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) = 0;
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
