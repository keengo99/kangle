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
#ifndef KHTTPAUTH_H
#define KHTTPAUTH_H
#include <stdlib.h>
/*
 * 定义认证类型
 */
#define AUTH_BASIC  0
#define AUTH_DIGEST 1
#define AUTH_NTLM   2
#ifdef ENABLE_DIGEST_AUTH
#define TOTAL_AUTH_TYPE 2
#else
#define TOTAL_AUTH_TYPE 1
#endif
#define AUTH_HEADER_AUTO 0
#define AUTH_HEADER_WEB  1
#define AUTH_HEADER_PROXY 2
#include "KStream.h"
#include "KHttpHeader.h"
#include "KFetchObject.h"
#include "global.h"
#include "katom.h"

#ifdef HTTP_PROXY
#define AUTH_REQUEST_HEADER "Proxy-Authorization"
#define AUTH_RESPONSE_HEADER "Proxy-Authenticate"
#define AUTH_STATUS_CODE 407
#define AUTH_HAS_FLAG      RQ_HAS_PROXY_AUTHORIZATION
#else
#define AUTH_REQUEST_HEADER "Authorization"
#define AUTH_RESPONSE_HEADER "WWW-Authenticate"
#define AUTH_STATUS_CODE 401
#define AUTH_HAS_FLAG     RQ_HAS_AUTHORIZATION
#endif


class KHttpRequest;
/*
 * http认证基类
 */
class KHttpAuth {
public:
	KHttpAuth(int type) {
		this->type = type;
		user = NULL;
		realm = NULL;
		auth_header = AUTH_HEADER_AUTO;
	}
	virtual ~KHttpAuth();
	static int parseType(const char *type);
	static const char *buildType(int type);
	int getType() {
		return type;
	}
	const char *getUser() {
		return user;
	}
	const char *getRealm() {
		return realm;
	}
	virtual KGL_RESULT response_header(kgl_output_stream* out) = 0;
	virtual void insertHeader(KHttpRequest *rq) = 0;
	virtual bool parse(KHttpRequest *rq, const char *str) = 0;
	virtual bool verify(KHttpRequest *rq, const char *password, int passwordType) = 0;
	virtual bool verifySession(KHttpRequest *rq)
	{
		return true;
	}
	void set_auth_header(uint16_t auth_header)
	{
		this->auth_header = auth_header;
	}
	uint16_t GetStatusCode()
	{
		switch (auth_header) {
		case AUTH_HEADER_WEB:
			return STATUS_UNAUTH;
		case AUTH_HEADER_PROXY:
			return STATUS_PROXY_UNAUTH;
		default:
			return AUTH_STATUS_CODE;
		}
	}
	friend class KAuthMark;
protected:
	const char *get_auth_header()
	{
		switch (auth_header) {
		case AUTH_HEADER_WEB:
			return "WWW-Authenticate";
		case AUTH_HEADER_PROXY:
			return "Proxy-Authenticate";
		default:
			return AUTH_RESPONSE_HEADER;
		}
	}
	char *user;
	char *realm;
private:
	uint16_t type;
	uint16_t auth_header;
};
class KAuthSource : public KFetchObject
{
public:
	KAuthSource(KHttpAuth *auth) {
		this->auth = auth;
	}
	~KAuthSource() {
		delete auth;
	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
	bool before_cache() override {
		return true;
	}
private:
	KHttpAuth* auth;
};
#endif
