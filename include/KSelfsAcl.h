#ifndef KSELFSACL_H
#define KSELFSACL_H
#include "KMultiAcl.h"
#include "KXml.h"
#include "utils.h"
class KSelfsAcl: public KMultiAcl {
public:
	KSelfsAcl() {
		icase_can_change = false;
		seticase(false);
	}
	virtual ~KSelfsAcl() {
	}
	KAcl *newInstance() {
		return new KSelfsAcl();
	}
	const char *getName() {
		return "selfs";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		char ip[MAXIPLEN];
		rq->sink->get_self_ip(ip,sizeof(ip));
		return KMultiAcl::match(ip);
	}
protected:
	char *transferItem(char *file)
	{
		return file;
	}
};
#endif
