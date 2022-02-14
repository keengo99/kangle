/*
 * KSocketFetchObject.h
 *
 *  Created on: 2010-4-23
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

#ifndef KSOCKETFETCHOBJECT_H_
#define KSOCKETFETCHOBJECT_H_
#if 0
#include "KFetchObject.h"
#include "KHttpObjectParserHook.h"

class KSocketFetchObject: public KFetchObject {
public:
	KSocketFetchObject();
	virtual ~KSocketFetchObject();
protected:
	int loadHead(KHttpRequest *rq, KHttpObject *obj,
			KHttpObjectParserHook &hook);
	KPoolableRedirect *as;
	KClientSocket *client;
};
#endif
#endif /* KSOCKETFETCHOBJECT_H_ */
