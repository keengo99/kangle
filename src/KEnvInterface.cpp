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
#include "KHttpLib.h"


KEnvInterface::KEnvInterface() {
}

KEnvInterface::~KEnvInterface() {
}
bool KEnvInterface::add_http_env(const char* attr, size_t attr_len, const char* val, size_t val_len)
{
	size_t dst_len = attr_len + 5;
	char* dst = (char*)xmalloc(dst_len + 1);
	dst[dst_len] = '\0';
	char* hot = dst;
	memcpy(hot, _KS("HTTP_"));
	hot += 5;
	while (attr_len > 0) {
		if (*attr == '-') {
			*hot = '_';
		} else {
			*hot = kgl_toupper(*attr);
		}
		attr++;
		hot++;
		attr_len--;
	}
	bool result = add_env(dst, dst_len, val, val_len);
	xfree(dst);
	return result;
}
bool KEnvInterface::add_http_header(const char* attr, size_t attr_len, const char* val, size_t val_len) {
	if (kgl_is_attr(attr, attr_len, _KS("Content-Type"))) {
		return add_content_type(val, val_len);
	}
	if (kgl_is_attr(attr, attr_len, _KS("Content-Length"))) {
		return add_content_length(val, val_len);
	}
	return add_http_env(attr, attr_len, val, val_len);
}
bool KEnvInterface::addEnv(const char* attr, int val) {
	char buf[16];
	snprintf(buf, 15, "%d", val);
	return addEnv(attr, buf);
}
bool KEnvInterface::add_content_type(const char* contentType, size_t len)
{
	return add_env(_KS("CONTENT_TYPE"), contentType, len);
}
bool KEnvInterface::add_content_length(const char* contentLength, size_t len)
{
	return add_env(_KS("CONTENT_LENGTH"), contentLength, len);
}
