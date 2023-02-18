/*
 * KHttpBasicAuth.h
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#ifndef KHTTPBASICAUTH_H_
#define KHTTPBASICAUTH_H_

#include "KHttpAuth.h"

class KHttpBasicAuth: public KHttpAuth {
public:
	KHttpBasicAuth();
	KHttpBasicAuth(const char *realm);
	virtual ~KHttpBasicAuth();
	bool parse(KHttpRequest *rq,const char *str) override;
	const char *getPassword()
	{
		return password;
	}
	bool verify(KHttpRequest *rq,const char *password,int passwordType) override;
	KGL_RESULT response_header(kgl_output_stream* out) override;
	void insertHeader(KHttpRequest *rq) override;
private:
	char *password;
};
#endif /* KHTTPBASICAUTH_H_ */
