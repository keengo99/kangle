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
	KAcl *new_instance() override {
		return new KSelfsAcl();
	}
	const char *getName() override {
		return "selfs";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		char ip[MAXIPLEN];
		rq->sink->get_self_ip(ip,sizeof(ip));
		return KMultiAcl::match(ip);
	}
protected:
	char *transferItem(char *file) override
	{
		return file;
	}
};
#endif
