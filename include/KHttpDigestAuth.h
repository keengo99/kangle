/*
 * KHttpDigestAuth.h
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#ifndef KHTTPDIGESTAUTH_H_
#define KHTTPDIGESTAUTH_H_
#include <map>
#include <list>
#include <time.h>
#include "utils.h"

#include "KHttpAuth.h"
#ifdef ENABLE_DIGEST_AUTH
#define MAX_SAVE_DIGEST_SESSION  120
#define MAX_SESSION_LIFETIME     300
class KHttpDigestSession {
public:
	KHttpDigestSession(const char *realm);
	~KHttpDigestSession();
	bool verify_rq(KHttpRequest *rq);

	/*
	 * realm
	 */
	char *realm;
	/*
	 * 最后活跃时间
	 */
	time_t lastActive;
	sockaddr_i addr;
};
class KHttpDigestAuth: public KHttpAuth {
public:
	KHttpDigestAuth();
	virtual ~KHttpDigestAuth();
	bool parse(KHttpRequest *rq, const char *str);
	bool verify(KHttpRequest *rq, const char *password, int passwordType);
	void insertHeader(KWStream &s);
	void init(KHttpRequest *rq,const char *realm);
	bool verifySession(KHttpRequest *rq);
	void insertHeader(KHttpRequest *rq);
	static void flushSession(time_t nowTime);
private:
	char *nonce;
	char *nc;
	char *cnonce;
	char *qop;
	char *response;
	char *uri;
	const char *orig_str;
	static std::map<char *, KHttpDigestSession *, lessp> sessions;
	static KMutex lock;
};
#endif
#endif /* KHTTPDIGESTAUTH_H_ */
