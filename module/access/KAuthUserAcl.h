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
#ifndef KAUTHUSERACL_H_
#define KAUTHUSERACL_H_

#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
class KAuthUserAcl: public KMultiAcl {
public:
	KAuthUserAcl() {
		icase = false;
		icase_can_change = false;
	}
	virtual ~KAuthUserAcl() {
	}
	KAcl *new_instance() override {
		return new KAuthUserAcl();
	}
	const char *getName() override {
		return "auth_user";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		if(rq->auth==NULL){
			return false;
		}
		const char *user = rq->auth->getUser();
		if(user==NULL){
			return false;
		}
		return KMultiAcl::match(user);
	}
};
class KRegAuthUserAcl: public KAcl
{
public:
	KRegAuthUserAcl()
	{
	}
	~KRegAuthUserAcl()
	{
	}
	const char *getName() override {
		return "reg_auth_user";
	}
	KAcl *new_instance() override {
		return new KRegAuthUserAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		if(rq->auth==NULL){
			return false;
		}
		const char *auth_user = rq->auth->getUser();
		if(auth_user==NULL){
			return false;
		}
		return user.match(auth_user,strlen(auth_user),0)>0;
	}
	std::string getHtml(KModel *model) override {
		KRegAuthUserAcl *m = (KRegAuthUserAcl *)model;
		std::stringstream s;
		s << "user regex:<input name='user' value='";
		if (m) {
			s << m->user.getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << user.getModel();
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		user.setModel(attribute["user"].c_str(),0);
	}
private:
	KReg user;
};
#endif /*KHOSTACL_H_*/
