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
	KAcl *new_instance() override {
		return new KSelfPortsAcl();
	}
	const char *getName() override {
		return "self_ports";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return KMultiIntAcl::match(rq->sink->get_self_port());
	}
};
class KListenPortsAcl: public KMultiIntAcl {
public:
	KListenPortsAcl() {

	}
	virtual ~KListenPortsAcl() {
	}
	KAcl *new_instance() override {
		return new KListenPortsAcl();
	}
	const char *getName() override {
		return "listen_ports";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		sockaddr_i addr = { 0 };
		rq->sink->get_self_addr(&addr);
		return KMultiIntAcl::match(ksocket_addr_port(&addr));
	}
};
#endif
