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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		gsl->addRef();
		rq->pushSpeedLimit(gsl);
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream& s) override {
		s << "limit: " << gsl->getSpeedLimit() ;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		gsl->setSpeedLimit((int)get_size(attribute["limit"].c_str()));
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "limit:<input name=limit size=10 value='";
		KGSpeedLimitMark *mark = (KGSpeedLimitMark *) (model);
		if (mark) {
			s << mark->gsl->getSpeedLimit();
		}
		s << "'>";
	}
	KMark * new_instance()override {
		return new KGSpeedLimitMark();
	}
	const char *getName()override {
		return "gspeed_limit";
	}
private:
	KSpeedLimit *gsl;
};

#endif /* KGSPEEDLIMITMARK_H_ */
