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
	KAcl *newInstance() {
		return new KAuthUserAcl();
	}
	const char *getName() {
		return "auth_user";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
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
	const char *getName() {
		return "reg_auth_user";
	}
	KAcl *newInstance() {
		return new KRegAuthUserAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if(rq->auth==NULL){
			return false;
		}
		const char *auth_user = rq->auth->getUser();
		if(auth_user==NULL){
			return false;
		}
		return user.match(auth_user,strlen(auth_user),0)>0;
	}
	std::string getHtml(KModel *model) {
		KRegAuthUserAcl *m = (KRegAuthUserAcl *)model;
		std::stringstream s;
		s << "user regex:<input name='user' value='";
		if (m) {
			s << m->user.getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay() {
		std::stringstream s;
		s << user.getModel();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute,bool html){
		user.setModel(attibute["user"].c_str(),0);
	}
	void buildXML(std::stringstream &s) {
		s << "user='" << user.getModel() << "'>";
	}
private:
	KReg user;
};
#endif /*KHOSTACL_H_*/
