/*
 * KHttpBasicAuth.cpp
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#include <string.h>
#include "utils.h"
#include "KHttpBasicAuth.h"
#include "kmalloc.h"
KHttpBasicAuth::KHttpBasicAuth() :
	KHttpAuth(AUTH_BASIC) {
	password = nullptr;
}
KHttpBasicAuth::KHttpBasicAuth(const char *realm) :
	KHttpAuth(AUTH_BASIC) {
	this->realm = xstrdup(realm);
	password = nullptr;
}

KHttpBasicAuth::~KHttpBasicAuth() {
	if (user) {
		xfree(user);
	}
	if (realm) {
		xfree(realm);
	}
}
void KHttpBasicAuth::insertHeader(KHttpRequest *rq)
{
	if (realm) {
		KStringBuf s;
		s << "Basic realm=\"" << realm  << "\"";
		const char *auth_header = this->get_auth_header();
		rq->response_header(auth_header, (hlen_t)strlen(auth_header) , s.getBuf(), s.getSize());
	}
}
KGL_RESULT KHttpBasicAuth::response_header(kgl_output_stream* out)
{
	if (realm) {
		KStringBuf s;
		s << "Basic realm=\"" << realm << "\"";
		const char* auth_header = this->get_auth_header();
		return out->f->write_unknow_header(out->ctx, auth_header, (hlen_t)strlen(auth_header), s.getBuf(), s.getSize());
	}
	return KGL_OK;
}
bool KHttpBasicAuth::parse(KHttpRequest *rq, const char *str) {
	int str_len = (int)strlen(str);
	if (str == NULL || str_len < 2) {
		return false;
	}
	user = b64decode((const unsigned char *) str, &str_len);
	if (user == NULL) {
		return false;
	}
	password = strchr(user, ':');
	if (password == NULL) {
		//Ã»ÓÐÃÜÂë
		return false;
	}
	*password = '\0';
	password++;
	return true;
}
bool KHttpBasicAuth::verify(KHttpRequest *rq, const char *password,
		int passwordType) {
	if (password == NULL || this->password == NULL) {
		return false;
	}
	return checkPassword(this->password, password, passwordType);
}
