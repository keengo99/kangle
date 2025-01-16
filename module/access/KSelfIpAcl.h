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
#ifndef KSELFIPACL_H_
#define KSELFIPACL_H_
#include "KIpAclBase.h"

class KSelfIpAcl : public KIpAclBase {
public:
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		sockaddr_i addr;
		if (!rq->sink->get_self_addr(&addr)) {
			return false;
		}
		ip_addr to;
		ksocket_ipaddr(&addr,&to);
		return matchIP(to);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		addIpModel(attribute["ip"].c_str(), ip);
	}
	void get_html(KWStream& s) override {
		s << "<input name=ip value='";
		KSelfIpAcl *acl2=static_cast<KSelfIpAcl *>(this);
		if (acl2) {
			acl2->get_display(s);
		}
		s << "'>(cidr format)";
	}
	KAcl *new_instance() override {
		return new KSelfIpAcl();
	}
	const char *get_module() const override {
		return "self";
	}
};
#endif /*KSRCACL_H_*/
