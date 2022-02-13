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
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=port value='";
		KDstPortAcl *urlAcl = (KDstPortAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KDstPortAcl();
	}
	const char *getName() {
		return "dst_port";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->url->port == port) {
			return true;
		}
		return false;
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
		if(character && isdigit(*character)){
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
