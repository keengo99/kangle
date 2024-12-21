/*
 * KAuthMark.h
 *
 *  Created on: 2010-4-28
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


#ifndef KAUTHACL_H_
#define KAUTHACL_H_
#include <map>
#include "KMark.h"
#include "global.h"
#include "KLineFile.h"
#include "KReg.h"
#include "KMutex.h"

class KAuthMark: public KMark {
public:
	KAuthMark();
	virtual ~KAuthMark();
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	void get_html(KWStream &s) override;
	void get_display(KWStream &s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	KString getRequireUsers();
	bool loadAuthFile(KString&path);
	bool checkLogin(KHttpRequest *rq);
	OpenState lastState;
	KString file;
	time_t lastModified;
	int cryptType;
	int auth_type;
	char *realm;
	KMutex lock;
	std::map<KString, KString> users;
	KReg *reg_user;
	bool reg_user_revert;
	bool all;
	bool file_sign;
	time_t lastLoad;
};

#endif /* KAUTHacl_H_ */
