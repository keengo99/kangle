/*
 * KRewriteMark.h
 *
 *  Created on: 2010-4-27
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

#ifndef KREWRITEMARK_H_
#define KREWRITEMARK_H_

#include "KMark.h"
#include "KReg.h"
#include "KRewriteMarkEx.h"

class KRewriteMark: public KMark {
public:
	KRewriteMark();
	virtual ~KRewriteMark();
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark * new_instance()override;
	const char *getName()override;
	void get_display(KWStream& s) override;
	void get_html(KModel* model, KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	KRewriteRule rule;
	KString prefix;
	int code;
};

#endif /* KREWRITEMARK_H_ */
