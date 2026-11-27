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
#include "katom.h"
#include "KSharedObj.h"
/*
 * Settings a mark hands to a body filter that keeps running after the
 * rules are checked. A named mark is re-parsed in place on reload, while
 * filters of running requests use it without the access lock. Such
 * settings live in a state object: parse_config builds a new one and
 * replaces it, a filter keeps a reference to the one it started with.
 */
class KMarkState {
public:
	KMarkState* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
protected:
	virtual ~KMarkState() {
	}
private:
	volatile int32_t ref = 1;
};
class KMark : public KModel
{
public:
	KMark() {
	}
	virtual KMark* new_instance() = 0;
	/* return KF_STATUS_REQ_FALSE, KF_STATUS_REQ_TRUE,KF_STATUS_REQ_FINISHED */
	virtual uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) = 0;
	/*
	 * Most marks run before the chain action is selected.  Response-body marks
	 * need to defer that action until body data is available, so they can
	 * override this overload and return KF_STATUS_REQ_DEFERRED.
	 */
	virtual uint32_t process_with_chain(KHttpRequest* rq, KHttpObject* obj,
		KSafeSource& fo, kgl_jump_type chain_jump_type) {
		return process(rq, obj, fo);
	}
protected:
	virtual ~KMark() {
	}
};
using KSafeMark = KSharedObj<KMark>;
#endif /*KMARK_H_*/
