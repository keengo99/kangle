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
#ifndef KDSTPORTACL_H_
#define KDSTPORTACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
class KDstPortAcl: public KAcl {
public:
	KDstPortAcl() {
		port = 80;
	}
	virtual ~KDstPortAcl() {
	}
	void get_html(KWStream& s) override {
		s << "<input name=port value='";		
		get_display(s);
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KDstPortAcl();
	}
	const char *get_module() const override {
		return "dst_port";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		if (rq->sink->data.url->port == port) {
			return true;
		}
		return false;
	}
	void get_display(KWStream& s) override {
		s << port;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if(attribute["port"].size()>0){
			port = atoi(attribute["port"].c_str());
		}
	}
private:
	int port;
};

#endif /*KHOSTACL_H_*/
