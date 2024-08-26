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
#ifndef KMETHODACL_H_
#define KMETHODACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KHttpKeyValue.h"
#include "KPathRedirect.h"
class KMethodAcl: public KAcl {
public:
	KMethodAcl() {
	}
	virtual ~KMethodAcl() {
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input name=meth placeholder='GET,POST,PUT,...' value='";
		KMethodAcl *urlAcl = (KMethodAcl *) (model);
		if (urlAcl) {
			urlAcl->get_display(s);
		}
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KMethodAcl();
	}
	const char *getName() override {
		return "meth";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return meth.matchMethod(rq->sink->data.meth);
	}
	void get_display(KWStream& s) override {
		meth.getMethod(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if(!attribute["meth"].empty()){
			meth.setMethod(attribute["meth"].c_str());
		} else {
			auto text = xml->get_text();
			if (!text.empty()) {
				meth.setMethod(text.c_str());
			}
		}
	}
private:
	KRedirectMethods meth;
};

#endif /*KHOSTACL_H_*/
