/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "KApiEnv.h"
#include "KHttpLib.h"
#include "ksapi.h"
#include "kmalloc.h"

using namespace std;
char *kgl_http_strdup(const char *val,size_t val_len)
{
	size_t len;
	char* copy;
	len = strnlen(val, val_len);
	copy = (char*)malloc(len + 1);
	if (copy) {
		char* end = copy + len;
		char* dst = copy;
		while (dst < end) {
			if (*val == '_') {
				*dst = '-';
			} else {
				*dst = kgl_toupper(*val);
			}
			val++;
			dst++;
		}
		*dst = '\0';
	}
	return copy;
}
KApiEnv::KApiEnv(void) {
	contentType = NULL;
	contentLength = 0;
}

KApiEnv::~KApiEnv(void) {
	std::map<char *, char *, lessp_icase >::iterator it;
	for (it = serverVars.begin(); it != serverVars.end(); it++) {
		xfree((*it).first);
		xfree((*it).second);
	}
	for (it = httpVars.begin(); it != httpVars.end(); it++) {
		xfree((*it).first);
		xfree((*it).second);
	}
	if (contentType) {
		xfree(contentType);
	}
}
bool KApiEnv::add_env_vars(const char *attr, const char *val, std::map<char *,
		char *, lessp_icase > &vars) {
	std::map<char *, char *, lessp_icase >::iterator it;
	it = vars.find((char *) attr);
	if (it != vars.end()) {
		return false;
	}
	vars.insert(pair<char *, char *> (xstrdup(attr), xstrdup(val)));
	return true;
}
bool KApiEnv::add_env(const char* attr, size_t attr_len, const char* val, size_t val_len)
{
	char* attr2 = kgl_strndup(attr, attr_len);
	std::map<char*, char*, lessp_icase >::iterator it;
	it = serverVars.find(attr2);
	if (it != serverVars.end()) {
		xfree(attr2);
		return false;
	}
	serverVars.insert(pair<char*, char*>(attr2, kgl_strndup(val,val_len)));
	return true;
}
bool KApiEnv::add_content_type(const char *contentType,size_t len) {
	if (this->contentType) {
		return false;
	}
	this->contentType = kgl_strndup(contentType,len);
	return KEnvInterface::add_content_type(contentType,len);
}
bool KApiEnv::add_content_length(const char *contentLength,size_t len) {
	this->contentLength = kgl_atol((u_char *)contentLength,len);
	return KEnvInterface::add_content_length(contentLength,len);
}
bool KApiEnv::add_http_env(const char *attr,size_t attr_len, const char *val,size_t val_len) {

	std::map<char *, char *, lessp_icase >::iterator it;
	char* http_attr = kgl_upstrndup(attr, attr_len);
	it = httpVars.find(http_attr);
	if (it != httpVars.end()) {
		xfree(http_attr);
		return false;
	}
	httpVars.insert(pair<char *, char *> (http_attr, kgl_strndup(val,val_len)));
	return true;
}
const char *KApiEnv::getHttpEnv(const char *attr) {
	char *buf = kgl_http_strdup(attr,strlen(attr));
#if 0
	printf("try found http env=[%s]\n", buf);
	for (auto it = httpVars.begin(); it != httpVars.end(); it++) {
		fprintf(stderr, "%s: %s\n", (*it).first, (*it).second);
	}
#endif
	const char * val = getHeaderEnv(buf);
	xfree(buf);
	return val;
}
const char *KApiEnv::getHeaderEnv(const char *attr) {
	std::map<char *, char *, lessp_icase >::iterator it;
	it = httpVars.find((char *) attr);
	if (it != httpVars.end()) {
		return (*it).second;
	}
	return NULL;
}
const char *KApiEnv::getEnv(const char *attr) {
	if (strncasecmp(attr, "HTTP_", 5) == 0) {
		return getHttpEnv(attr + 5);
	}
	if (strncasecmp(attr, "HEADER_", 7) == 0) {
		return getHeaderEnv(attr + 7);
	}
	std::map<char *, char *, lessp_icase >::iterator it;
	it = serverVars.find((char *) attr);
	if (it != serverVars.end()) {
		return (*it).second;
	}
	return NULL;
}
bool KApiEnv::getAllHttp(char *buf, int *buf_size) {
	//	int len = *buf_size;
	int totalLen = 0;
	std::map<char *, char *, lessp_icase >::iterator it;
	for (it = httpVars.begin(); it != httpVars.end(); it++) {
		char *attr = (*it).first;
		char *val = (*it).second;
		int len = (int)(strlen(attr) + strlen(val) + 9);
		totalLen += len;
		if (*buf_size <= totalLen) {
			continue;
		}
		if(buf==NULL){
			continue;
		}
		strncpy(buf, "HTTP_", 5);
		buf += 5;
		while (*attr) {
			if (*attr == '-') {
				*buf = '_';
			} else {
				*buf = *attr;
			}
			buf++;
			attr++;
		}
		strncpy(buf, ": ", 2);
		buf += 2;
		int val_len = (int)strlen(val);
		strncpy(buf, val, val_len);
		buf += val_len;
		strcpy(buf, "\r\n");
		buf += 2;
		//*buf_size -= len;
	}
	if(*buf_size <= totalLen){
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		*buf_size = totalLen+1;
		return false;
	}
	*buf_size = totalLen+1;
	if(buf){
		*buf = '\0';
	}
	return true;
}
bool KApiEnv::getAllRaw(KStringBuf &s)
{
	std::map<char *, char *, lessp_icase >::iterator it;
	for (it = httpVars.begin(); it != httpVars.end(); it++) {
		s << (*it).first << ": " << (*it).second << "\r\n";
	}
	return true;
}
