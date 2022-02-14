#ifndef KVHACL_H_DDD
#define KVHACL_H_DDD
#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
class KVhAcl: public KMultiAcl {
public:
	KVhAcl() {
		icase_can_change = false;
	}
	virtual ~KVhAcl() {
	}
	KAcl *newInstance() {
		return new KVhAcl();
	}
	const char *getName() {
		return "vh";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->svh) {
			return KMultiAcl::match(rq->svh->vh->name.c_str());
		}
		return false;		
	}
};
#endif
