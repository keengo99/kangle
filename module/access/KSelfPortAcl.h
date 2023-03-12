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
#ifndef KSELFPORTACL_H_
#define KSELFPORTACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
class KSelfPortAcl: public KAcl {
public:
	KSelfPortAcl() {
		port = 80;
	}
	virtual ~KSelfPortAcl() {
	}
	void get_html(KModel *model,KWStream &s) override {
		s << "<input name=port value='";
		KSelfPortAcl *urlAcl = (KSelfPortAcl *) (model);
		if (urlAcl) {
			urlAcl->get_display(s);
		}
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KSelfPortAcl();
	}
	const char *getName() override {
		return "self_port";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		return rq->sink->get_self_port() == port;
	}
	void get_display(KWStream &s) override {
		s << port;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if(attribute["port"].size()>0){
			port = atoi(attribute["port"].c_str());
		} else {
			port = atoi(xml->get_text());
		}
	}
private:
	int port;
};

#endif /*KHOSTACL_H_*/
