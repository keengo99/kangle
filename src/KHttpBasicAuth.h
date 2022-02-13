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
	bool parse(KHttpRequest *rq,const char *str);
	const char *getPassword()
	{
		return password;
	}
	bool verify(KHttpRequest *rq,const char *password,int passwordType);
	void insertHeader(KWStream &s);
	void insertHeader(KHttpRequest *rq);
private:
	char *password;
};
#endif /* KHTTPBASICAUTH_H_ */
