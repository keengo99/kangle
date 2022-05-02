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
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		sockaddr_i addr;
		if (!rq->sink->get_self_addr(&addr)) {
			return false;
		}
		ip_addr to;
		ksocket_ipaddr(&addr,&to);
		return matchIP(to);
	}
	;
	void editHtml(std::map<std::string,std::string> &attribute,bool html){
		addIpModel(attribute["ip"].c_str(), ip);
	}
	;
	std::string getHtml(KModel *acl) {
		std::stringstream s;
		s << "<input name=ip value='";
		KSelfIpAcl *acl2=(KSelfIpAcl *)(acl);
		if (acl2) {
			s << acl2->getDisplay();
		}
		s << "'>(cidr format)";
		return s.str();
	}
	KAcl *newInstance() {
		return new KSelfIpAcl();
	}
	const char *getName() {
		return "self";
	}
};
#endif /*KSRCACL_H_*/
