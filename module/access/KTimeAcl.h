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
#ifndef KTIMEACL_H_
#define KTIMEACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KTimeMatch.h"
#include "time_utils.h"
class KTimeAcl: public KAcl {
public:
	KTimeAcl() {
	}
	virtual ~KTimeAcl() {
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "<input name='time' value='";
		KTimeAcl *urlAcl = (KTimeAcl *) (model);
		if (urlAcl) {
			s << urlAcl->ts;
		}
		s << "'>";
		return s.str();
	}
	KAcl *new_instance() override {
		return new KTimeAcl();
	}
	const char *getName() override {
		return "time";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		return t.checkTime(kgl_current_sec);
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << ts;
		return s.str();
	}
	bool startCharacter(KXmlContext *context, char *character, int len) override {
		if (character) {
			ts = character;
			t.set(character);
		}
		return true;
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override {
		ts = attribute["time"];
		t.set(ts.c_str());
	}
	void buildXML(std::stringstream &s) override {
		s << " time='" << ts << "'>";
	}
private:
	std::string ts;
	KTimeMatch t;
};

#endif /*KHOSTACL_H_*/
