/*
 * KGSpeedLimitMark.h
 *
 *  Created on: 2010-5-30
 *      Author: keengo
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


#ifndef KGSPEEDLIMITMARK_H_
#define KGSPEEDLIMITMARK_H_
#include<string>
#include<map>
#include "KMark.h"
#include "do_config.h"
class KGSpeedLimitMark: public KMark {
public:
	KGSpeedLimitMark() {
		gsl = new KSpeedLimit;
	}
	virtual ~KGSpeedLimitMark() {
		gsl->release();
	}
	bool supportRuntime() override
	{
		return true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo)override {
		gsl->addRef();
		rq->pushSpeedLimit(gsl);
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << "limit: " << gsl->getSpeedLimit() ;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override {
		gsl->setSpeedLimit((int)get_size(attribute["limit"].c_str()));
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "limit:<input name=limit size=10 value='";
		KGSpeedLimitMark *mark = (KGSpeedLimitMark *) (model);
		if (mark) {
			s << mark->gsl->getSpeedLimit();
		}
		s << "'>";
		return s.str();
	}
	KMark * new_instance()override {
		return new KGSpeedLimitMark();
	}
	const char *getName()override {
		return "gspeed_limit";
	}
public:
	void buildXML(std::stringstream &s)override {
		s << " limit='" << gsl->getSpeedLimit() << "'>";
	}
private:
	KSpeedLimit *gsl;
};

#endif /* KGSPEEDLIMITMARK_H_ */
