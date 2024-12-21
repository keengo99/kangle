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
#ifndef KOBJALWAYSONACL_H_
#define KOBJALWAYSONACL_H_

#include "KAcl.h"
#include "KXml.h"
class KObjAlwaysOnAcl: public KAcl {
public:
	KObjAlwaysOnAcl() {
	}
	virtual ~KObjAlwaysOnAcl() {
	}
	void get_html(KWStream& s) override {
		return;
	}
	KAcl *new_instance() override {
		return new KObjAlwaysOnAcl();
	}
	const char *getName() override {
		return "obj_always_on";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return rq->ctx.always_on_model;
	}
	void get_display(KWStream& s) override {
	}

	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();		
	}
private:
};

#endif
