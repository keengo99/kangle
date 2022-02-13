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
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=meth placeholder='GET,POST,PUT,...' value='";
		KMethodAcl *urlAcl = (KMethodAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KMethodAcl();
	}
	const char *getName() {
		return "meth";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return meth.matchMethod(rq->meth);
	}
	std::string getDisplay() {
		std::stringstream s;
		s << meth.getMethod();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) {
		if(!attribute["meth"].empty()){
			meth.setMethod(attribute["meth"].c_str());
		}
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if(len>0){
			meth.setMethod(character);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << ">" << meth.getMethod();
	}
private:
	KRedirectMethods meth;
};

#endif /*KHOSTACL_H_*/
