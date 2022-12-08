#ifndef KSELFPORTSACL_H
#define KSELFPORTSACL_H
#include "KMultiIntAcl.h"
#include "KXml.h"
class KSelfPortsAcl: public KMultiIntAcl {
public:
	KSelfPortsAcl() {

	}
	virtual ~KSelfPortsAcl() {
	}
	KAcl *newInstance() {
		return new KSelfPortsAcl();
	}
	const char *getName() {
		return "self_ports";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return KMultiIntAcl::match(rq->sink->get_self_port());
	}
};
class KListenPortsAcl: public KMultiIntAcl {
public:
	KListenPortsAcl() {

	}
	virtual ~KListenPortsAcl() {
	}
	KAcl *newInstance() {
		return new KListenPortsAcl();
	}
	const char *getName() {
		return "listen_ports";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		sockaddr_i addr = { 0 };
		rq->sink->get_self_addr(&addr);
		return KMultiIntAcl::match(ksocket_addr_port(&addr));
	}
};
#endif
