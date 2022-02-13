/*
* KEnvInterface.cpp
*
*  Created on: 2010-6-11
*      Author: keengo
*/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "KEnvInterface.h"
#include "kmalloc.h"

KEnvInterface::KEnvInterface() {
}

KEnvInterface::~KEnvInterface() {
}
bool KEnvInterface::addHttpEnv(const char *attr,const char *val)
{
	int len = strlen(attr);
	char *dst = (char *) xmalloc(len + 6);
	char *hot = dst;
	strncpy(hot, "HTTP_", 5);
	hot += 5;
	while (*attr) {
		if (*attr == '-') {
			*hot = '_';
		} else {
			*hot = toupper(*attr);
		}
		attr++;
		hot++;
	}
	*hot = '\0';
	bool result = addEnv(dst, val);
	xfree(dst);
	return result;
}
bool KEnvInterface::addHttpHeader(char *attr, char *val) {
	if(strcasecmp(attr,"Content-Type")==0){
		return addContentType(val);
	}
	if(strcasecmp(attr,"Content-Length")==0){
		return addContentLength(val);
	}
	return addHttpEnv(attr,val);
}
bool KEnvInterface::addEnv(const char *attr, int val) {
	char buf[16];
	snprintf(buf, 15, "%d", val);
	return addEnv(attr, buf);
}
bool KEnvInterface::addContentType(const char *contentType)
{
	return addEnv("CONTENT_TYPE",contentType);
}
bool KEnvInterface::addContentLength(const char *contentLength)
{
	return addEnv("CONTENT_LENGTH",contentLength);
}
