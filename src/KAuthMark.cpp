/*
 * KAuthMark.cpp
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

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sys/stat.h>
#include "KAuthMark.h"
#include "do_config.h"
#include "KVirtualHost.h"
#include "utils.h"
#include "kmd5.h"
#include "lang.h"
#include "kforwin32.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"
#include "KLineFile.h"
#include "kmalloc.h"
#define AUTH_FILE_SIGN  "--AUTH_FILE_SIGN--"
#ifdef ENABLE_HTPASSWD_CRYPT
int apr_password_validate(const char *passwd,const char *hash);
#endif
using namespace std;
bool checkSaltMd5(const char *toCheck, const char *password) {
	int password_len = strlen(password);
	if (password_len<32) {
		return false;
	}
	KMD5_CTX context;
	unsigned char digest[17];
	KMD5Init (&context);
	KMD5Update(&context,(unsigned char *)toCheck, strlen(toCheck));
	KMD5Update(&context,(unsigned char *)password+32,password_len-32);
	KMD5Final (digest, &context);
	char buf[33];
	make_digest(buf,digest);
	return strncasecmp(buf,password,32)==0;
}
bool checkPassword(const char *toCheck, const char *password, int cryptType) {
	char buf[65];
	switch (cryptType) {
	case CRYPT_TYPE_KMD5:
		KMD5(toCheck,strlen(toCheck), buf);
		return strcasecmp(buf, password) == 0;
	case CRYPT_TYPE_PLAIN:
		return strcmp(toCheck, password) == 0;
#ifdef ENABLE_HTPASSWD_CRYPT
	case CRYPT_TYPE_HTPASSWD:
		//return apr_password_validate(toCheck,password) == 0;
#endif
	case CRYPT_TYPE_SALT_MD5:	
		return checkSaltMd5(toCheck,password);
	case CRYPT_TYPE_SIGN:
	{
			if (*password=='$') {
				//$plain
				return strcmp(toCheck,password+1)==0;
			}
			if (*password=='#') {
				//#md5
				return checkSaltMd5(toCheck,password+1);
			}
			//time_limit-skey
			//create_time-sign
			//sign=md5(create_time+skey)
			INT64 create_time = string2int(toCheck);
			INT64 time_limit = string2int(password);
			INT64 time_diff = time(NULL) - create_time;
			if (time_diff < -86400) {
				klog(KLOG_ERR,"password time in future\n");
				return false;
			}
			if (time_diff > time_limit) {
				klog(KLOG_ERR,"password time expire\n");
				return false;
			}
			const char *user_sign = strchr(toCheck,'-');
			const char *skey = strchr(password,'-');
			if (user_sign==NULL || skey==NULL) {
				klog(KLOG_ERR,"user sign or skey format error(time-sign)\n");
				return false;
			}
			KMD5_CTX context;
 			unsigned char digest[17];
 			KMD5Init (&context);
  			KMD5Update(&context,(unsigned char *)toCheck, user_sign - toCheck);
			user_sign++;
			skey++;
			KMD5Update(&context,(unsigned char *)skey,strlen(skey));
			KMD5Final (digest, &context);
			char buf[33];
			make_digest(buf,digest);
			return strncasecmp(buf,user_sign,32)==0;
	}
	default:
		klog(KLOG_ERR, "unknow cryptType=%d\n", cryptType);
		return false;
	}
}
KAuthMark::KAuthMark() {
	lastModified = 0;
	cryptType = CRYPT_TYPE_PLAIN;
	lastState = OPEN_UNKNOW;
	auth_type = AUTH_BASIC;
	realm = NULL;
	all = true;
	reg_user = NULL;
	reg_user_revert = false;
	failed_deny = true;
	file_sign = false;
	lastLoad = 0;
}

KAuthMark::~KAuthMark() {
	if(realm){
		xfree(realm);
	}
	if (reg_user) {
		delete reg_user;
	}
}
bool KAuthMark::mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType) {
	std::string path;
#ifndef HTTP_PROXY
	auto svh = rq->get_virtual_host();
	if (svh && !this->isGlobal)
		path = svh->vh->doc_root;
	else 
#endif
		path = conf.path;

	if (!loadAuthFile(path)) {
		klog(KLOG_ERR, "cann't load auth file[%s]\n", path.c_str());
		if (failed_deny) {
			jumpType = JUMP_DENY;
			return true;
		}
		return false;
	}
	if (checkLogin(rq)) {
		return true;
	}
	if (rq->auth) {
		delete rq->auth;
		rq->auth = NULL;
	}
	if (auth_type == AUTH_BASIC) {
		KHttpBasicAuth *auth = new KHttpBasicAuth();		
		auth->realm = xstrdup(realm?realm:PROGRAM_NAME);
		rq->auth = auth;
	} else if (auth_type == AUTH_DIGEST) {
#ifdef ENABLE_DIGEST_AUTH
		KHttpDigestAuth *auth = new KHttpDigestAuth();
		auth->init(rq,realm?realm:PROGRAM_NAME);
		rq->auth = auth;
#endif
	}
	jumpType = JUMP_DENY;
	KBIT_SET(rq->filter_flags,RQ_SEND_AUTH);
	return true;

}
bool KAuthMark::checkLogin(KHttpRequest *rq) {
	if (rq->auth == NULL || rq->auth->getUser() == NULL) {
		return false;
	}
	if (rq->auth->getType() != auth_type) {
		return false;
	}
	bool result = false;
	lock.Lock();
	map<string, string>::iterator it;
	it = users.find(rq->auth->getUser());
	if (it==users.end()) {
		it = users.find("*");
	}
	if (it != users.end() && (*it).second.size()>0) {
		result = rq->auth->verify(rq, (*it).second.c_str(), cryptType);
	}
	lock.Unlock();
	return result;
}
bool KAuthMark::loadAuthFile(std::string &path) {
	INT64 diffTime = kgl_current_sec - lastLoad;
	if (diffTime>-10 && diffTime<10) {
		return lastState!=OPEN_FAILED;
	}
	lastLoad = kgl_current_sec;
	if(!isAbsolutePath(file.c_str())){
		path += "/";
		path += file;
	}else{
		path = file;
	}
	KLineFile lFile;
	std::map<std::string,std::string> users;
	lock.Lock();
	lastState = lFile.open(path.c_str(), lastModified);
	if (lastState == OPEN_NOT_MODIFIED) {
		lock.Unlock();
		return true;
	}
	//users.clear();
	if (lastState == OPEN_FAILED) {
		lock.Unlock();
		return false;
	}
	bool file_signed = false;
	for (;;) {
		char *hot = lFile.readLine();
		if (hot == NULL) {
			break;
		}
		if (strcmp(hot,AUTH_FILE_SIGN)==0) {
			file_signed = true;
			break;
		}
		char *p = strchr(hot,':');
		if (p==NULL) {
			fprintf(stderr, "user passwd format error[%s]\n", hot);
			break;
		}
		*p = '\0';
		char *user = hot;
		char *passwd = p+1;
		if(!all){
			if (reg_user) {
				if ((reg_user->match(user,strlen(user),0)>0) != reg_user_revert) {
					users.insert(pair<string, string> (user, passwd));
				}
			} else {
				std::map<std::string,std::string>::iterator it;
				it = users.find(user);
				if(it!=users.end()){
					(*it).second = passwd;
				}
			}
		}else{
			users.insert(pair<string, string> (user, passwd));
		}
	}
	if (!this->file_sign || (this->file_sign && file_signed)) {
		this->users.swap(users);
	}
	lock.Unlock();
	return true;
}
KMark *KAuthMark::newInstance() {
	return new KAuthMark;
}
const char *KAuthMark::getName() {
	return "auth";
}
std::string KAuthMark::getHtml(KModel *model) {
	std::stringstream s;
	KAuthMark *acl = (KAuthMark *) model;
	s << "file:<input name='file' value='";
	if (acl) {
		s << acl->file;
	}
	s << "'>crypt type:<select name='crypt_type'>";
	for (int i = 0; i < TOTAL_CRYPT_TYPE; i++) {
		s << "<option value='" << buildCryptType(i) << "' ";
		if (acl && i == acl->cryptType) {
			s << "selected";
		}
		s << ">" << buildCryptType(i) << "</option>";
	}
	s << "</select><br>";
	s << "auth type:";
	for (int i = 0; i < TOTAL_AUTH_TYPE; i++) {
		s << "<input type=radio name='auth_type' value='"
				<< KHttpAuth::buildType(i) << "' ";
		if (auth_type == i) {
			s << "checked";
		}
		s << ">" << KHttpAuth::buildType(i) << " ";
	}
	s << "<br>realm:<input name='realm' value='" << (realm?realm:PROGRAM_NAME) << "'>";
	s << "<br>Require:<input name='require' value='" << getRequireUsers() << "'>";
	s << "<br><input type='checkbox' name='failed_deny' value='1' ";
	if (acl && acl->failed_deny) {
		s << "checked";
	}
	s << ">failed_deny";
	s << "<input type='checkbox' name='file_sign' value='1' ";
	if (acl && acl->file_sign) {
		s << "checked";
	}
	s << ">file_sign";
	return s.str();
}
std::string KAuthMark::getRequireUsers()
{
	std::stringstream s;
	if(all){
		s << "*";
	}else{
		if (reg_user) {
			s << "~";
			if (reg_user_revert) {
				s << "!";
			}
			s << reg_user->getModel();
		} else {
			lock.Lock();
			std::map<std::string,std::string>::iterator it;
			for(it=users.begin();it!=users.end();it++){
				if(it!=users.begin()){
					s << ",";
				}
				s << (*it).first;
			}
			lock.Unlock();
		}
	}
	return s.str();
}
std::string KAuthMark::getDisplay() {
	stringstream s;
	s << "realm:" << (realm?realm:"") << " ";
	s << KHttpAuth::buildType(auth_type) << " ";
	s << "file:" << file << " crypt_type:" << buildCryptType(cryptType);
	s << " read state:";
	switch (lastState) {
	case OPEN_UNKNOW:
		s << "unknow";
		break;
	case OPEN_FAILED:
		s << "<font color='red'>failed</font>";
		break;
	case OPEN_SUCCESS:
	case OPEN_NOT_MODIFIED:
		s << "<font color='green'>success</font>";
	}
	s << ",require:" << getRequireUsers();
	return s.str();
}
void KAuthMark::editHtml(std::map<std::string, std::string> &attribute,bool html)
		 {
	file = attribute["file"];
	lock.Lock();
	cryptType = parseCryptType(attribute["crypt_type"].c_str());
	lastModified = 0;
	auth_type = KHttpAuth::parseType(attribute["auth_type"].c_str());
	failed_deny = (attribute["failed_deny"]=="1");
	file_sign = (attribute["file_sign"]=="1");
	if(realm){
		xfree(realm);
		realm = NULL;
	}
	if(attribute["realm"].size()>0){
		realm = xstrdup(attribute["realm"].c_str());
	}
	string require = attribute["require"];
	users.clear();
	if (reg_user) {
		delete reg_user;
		reg_user = NULL;
	}
	if(require.size()==0 || require=="*"){
		all = true;
	}else{
		all = false;
		char *str = xstrdup(require.c_str());
		char *buf = str;
		if (*buf=='~') {
			reg_user = new KReg;
			buf++;
			if (*buf=='!') {
				reg_user_revert = true;
				buf++;
			} else {
				reg_user_revert = false;
			}
			reg_user->setModel(buf,0);
		} else {
			for(;;){
				char *hot = strchr(buf,',');
				if(hot!=NULL){
					*hot = '\0';
				}
				users.insert(pair<std::string,std::string>(buf,""));
				if(hot==NULL){
					break;
				}
				buf = hot+1;
			}
		}
		xfree(str);
	}
	lock.Unlock();
}
void KAuthMark::buildXML(std::stringstream &s) {
	s << " file='" << file << "'";
	s << " crypt_type='" << buildCryptType(cryptType) << "'";
	s << " auth_type='" << KHttpAuth::buildType(auth_type) << "'";
	s << " realm='" << (realm?realm:PROGRAM_NAME) << "'";
	s << " require='" << getRequireUsers() << "'";
	s << " failed_deny='" << (failed_deny?1:0) << "'";
	if (file_sign) {
		s << " file_sign='1'";
	}
	s << ">";

}
