/*
 * KSockFastcgiFetchObject.h
 *
 *  Created on: 2010-6-15
 *      Author: keengo
 */

#ifndef KSOCKFASTCGIFETCHOBJECT_H_
#define KSOCKFASTCGIFETCHOBJECT_H_

#include "KFastcgiFetchObject.h"
#if 0
class KSockFastcgiFetchObject: public KFastcgiFetchObject {
public:
	KSockFastcgiFetchObject(KPoolableRedirect *as, KFileName *file);
	virtual ~KSockFastcgiFetchObject();
	bool enable() {
		return as->enable;
	}
protected:
	bool createConnection(KHttpRequest *rq, KHttpObject *obj, bool mustCreate);
	bool freeConnection(KHttpRequest *rq, bool realClose);
private:
	KPoolableRedirect *as;
};
#endif
#endif /* KSOCKFASTCGIFETCHOBJECT_H_ */
