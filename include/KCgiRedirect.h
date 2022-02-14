/*
 * KCgiRedirect.h
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */

#ifndef KCGIREDIRECT_H_
#define KCGIREDIRECT_H_
#include <vector>
#include "KRedirect.h"
#ifdef ENABLE_CGI
class KCgiRedirect: public KRedirect {
public:
	KCgiRedirect(const char *cmd);
	KCgiRedirect();
	virtual ~KCgiRedirect();
	bool setArg(std::string &path);
	bool setEnv(std::string &env, std::string &split);
	std::string getEnv();
	std::string getArg();
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	void buildXML(std::stringstream &s);
	friend class KCgiFetchObject;
	friend class KAcserverManager;
	const char *getType(){
		return "cgi";
	}
private:
	char *cmd;
	std::vector<std::string> args;
	std::vector<std::string> envs;
	char split_char;
};
extern KCgiRedirect globalCgi;
#endif
#endif /* KCGIREDIRECT_H_ */
