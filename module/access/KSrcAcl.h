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
#ifndef KSRCACL_H_
#define KSRCACL_H_
#include "KIpAclBase.h"

class KSrcAcl : public KIpAclBase {
public:
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		ip_addr addr;
		if (!ksocket_get_ipaddr(rq->getClientIp(), &addr)) {
			return false;
		}
		return matchIP(addr);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		addIpModel(attribute["ip"].c_str(), ip);
	}
	void get_html(KWStream& s) override {
		s << "<input name=ip value='";		
		this->get_display(s);		
		s << "'>(cidr format)";
	}
	KAcl *new_instance() override {
		return new KSrcAcl();
	}
	const char *getName() override {
		return "src";
	}
};
#endif /*KSRCACL_H_*/
