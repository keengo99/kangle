/*
 * KFilter.h
 *
 *  Created on: 2010-5-9
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#ifndef KFILTER_H_
#define KFILTER_H_
#include "KMark.h"
#define PARTIALMATCH_BACKLEN	32
enum {
	FILTER_MATCH, FILTER_PARTIALMATCH, FILTER_NOMATCH,
};
class KFilterKey {
public:
	KFilterKey() {
		key = NULL;
		next = NULL;
	}
	~KFilterKey() {
		if (key) {
			xfree(key);
		}
	}
	char *key;
	int len;
	KFilterKey *next;

};
class KContentMark: public KMark {
public:
	virtual ~KContentMark() {

	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType);

	/*
	 * filter the content.
	 */
	virtual int check(const char *buf, int len, const char *charset,
			KFilterKey **keys) = 0;
};

#endif /* KFILTER_H_ */
