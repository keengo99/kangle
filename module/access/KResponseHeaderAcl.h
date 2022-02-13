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
#ifndef KRESPONSEHEADERACL_H_
#define KRESPONSEHEADERACL_H_
#include "KHeaderAcl.h"
class KResponseHeaderAcl : public KHeaderAcl {
	KAcl *newInstance() {
		return new KResponseHeaderAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if(obj->data==NULL){
			return false;
		}
		return matchHeader(obj->data->headers);
	}
};
#endif /*KRESPONSEHEADERACL_H_*/
