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
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "<input name=meth placeholder='GET,POST,PUT,...' value='";
		KMethodAcl *urlAcl = (KMethodAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *new_instance() override {
		return new KMethodAcl();
	}
	const char *getName() override {
		return "meth";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj)  override {
		return meth.matchMethod(rq->sink->data.meth);
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << meth.getMethod();
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if(!attribute["meth"].empty()){
			meth.setMethod(attribute["meth"].c_str());
		}
	}
private:
	KRedirectMethods meth;
};

#endif /*KHOSTACL_H_*/
