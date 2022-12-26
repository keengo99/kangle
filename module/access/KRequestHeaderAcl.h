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
#ifndef KREQUESTHEADERACL_H_
#define KREQUESTHEADERACL_H_
#include "KHeaderAcl.h"
class KRequestHeaderAcl : public KHeaderAcl {
	KAcl *newInstance() {
		return new KRequestHeaderAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return match_header(rq->sink->data.get_header());
	}
};
#endif /*KREQUESTHEADERACL_H_*/
