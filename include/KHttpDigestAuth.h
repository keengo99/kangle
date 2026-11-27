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
/* Every 401 challenge creates a session, so an unauthenticated client could
 * otherwise grow the table without limit. The least recently used go first. */
#define MAX_DIGEST_SESSION       65536
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
	/* position in KHttpDigestAuth::lru */
	std::list<char *>::iterator lru;
};
class KHttpDigestAuth: public KHttpAuth {
public:
	KHttpDigestAuth();
	virtual ~KHttpDigestAuth();
	bool parse(KHttpRequest *rq, const char *str) override;
	bool verify(KHttpRequest *rq, const char *password, int passwordType) override;
	KGL_RESULT response_header(kgl_output_stream* out) override;
	void init(KHttpRequest *rq,const char *realm);
	bool verifySession(KHttpRequest *rq) override;
	void insertHeader(KHttpRequest *rq) override;
	static void flushSession(time_t nowTime);
	void set_stale(bool stale)
	{
		this->stale = stale;
	}
private:
	static void remove_session(std::map<char *, KHttpDigestSession *, lessp>::iterator it);
	bool stale;
	char *nonce;
	char *nc;
	char *cnonce;
	char *qop;
	char *response;
	char *uri;
	const char *orig_str;
	static std::map<char *, KHttpDigestSession *, lessp> sessions;
	/* session keys, least recently used first */
	static std::list<char *> lru;
	static KMutex lock;
};
#endif
#endif /* KHTTPDIGESTAUTH_H_ */
