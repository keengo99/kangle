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
#ifndef KSPEEDLIMITMARK_H_
#define KSPEEDLIMITMARK_H_
#include<string>
#include<map>
#include "KMark.h"
#include "do_config.h"
#include "KSpeedLimit.h"

class KSpeedLimitMark : public KMark {
public:
	KSpeedLimitMark() {
		speed_limit=0;
	}	
	virtual ~KSpeedLimitMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override {
		KSpeedLimit *sl = new KSpeedLimit;
		sl->setSpeedLimit(speed_limit);
		rq->pushSpeedLimit(sl);
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << speed_limit;
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html)override {
		speed_limit=(int)get_size(attribute["limit"].c_str());
	}
	std::string getHtml(KModel *model) override {
		KSpeedLimitMark *mark = (KSpeedLimitMark *)model;
		std::stringstream s;
		s << "limit:<input name='limit' size=10 value='";
		if (mark) {
			s << mark->speed_limit;
		}
		s << "'>";
		return s.str();
	}
	KMark * new_instance()override {
		return new KSpeedLimitMark();
	}
	const char *getName()override {
		return "speed_limit";
	}
public:
	void buildXML(std::stringstream &s) override {
		s << " limit='" << speed_limit << "'>";
	}
private:
	int speed_limit;
};
#endif /*KSPEEDLIMITMARK_H_*/
