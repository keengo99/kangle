/*
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
#ifndef KMARK_H_
#define KMARK_H_
#include "KModel.h"
#include "KBuffer.h"
#define 	MARK_CONTEXT		"mark"
class KHttpRequest;
class KHttpObject;
class KFetchObject;
/**
 * ±ê¼Ç
 */
class KMark: public KModel {
public:
	KMark() {
	};
	virtual ~KMark() {
	};
	virtual bool mark(KHttpRequest* rq, KHttpObject* obj, KFetchObject **fo) = 0;
	virtual KMark* new_instance() = 0;
	virtual int32_t shutdown()
	{
		return 0;
	}
};
#endif /*KMARK_H_*/
