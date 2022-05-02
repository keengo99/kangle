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
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=port value='";
		KSelfPortAcl *urlAcl = (KSelfPortAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KSelfPortAcl();
	}
	const char *getName() {
		return "self_port";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return rq->sink->get_self_port() == port;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << port;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) {
		if(attribute["port"].size()>0){
			port = atoi(attribute["port"].c_str());
		}
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if (character && isdigit(character[0])) {
			port = atoi(character);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << ">" << port ;
	}
private:
	int port;
};

#endif /*KHOSTACL_H_*/
