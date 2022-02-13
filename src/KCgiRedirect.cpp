/*
 * KCgiRedirect.cpp
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */
#include <vector>
#include "KCgiFetchObject.h"
#include "KCgiRedirect.h"
#include "kmalloc.h"
#include "utils.h"
#ifdef ENABLE_CGI
KCgiRedirect globalCgi;
using namespace std;
KCgiRedirect::KCgiRedirect() {
	cmd = NULL;
}
KCgiRedirect::KCgiRedirect(const char *cmd) {

	this->cmd = xstrdup(cmd);
	split_char = '|';
}

KCgiRedirect::~KCgiRedirect() {
	if (cmd) {
		xfree(cmd);
	}
}
bool KCgiRedirect::setArg(std::string &arg) {
	if (arg.size() > 0) {
		explode(arg.c_str(), ' ', args);
	}
	return true;
}
std::string KCgiRedirect::getArg()
{
	stringstream s;
	for(size_t i=0;i<args.size();i++){
		if(i>0){
			s << " ";
		}
		s << args[i];
	}
	return s.str();
}
std::string KCgiRedirect::getEnv() {
	stringstream s;
	for (size_t i = 0; i < envs.size(); i++) {
		if (i > 0) {
			s << split_char;
		}
		s << envs[i];
	}
	return s.str();
}
bool KCgiRedirect::setEnv(std::string &env, std::string &split) {
	if (split.size() > 0) {
		split_char = split[0];
	}
	explode(env.c_str(), split_char, envs);
	return true;
}
KFetchObject *KCgiRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	KCgiFetchObject *fo = new KCgiFetchObject;
	return fo;
}
void KCgiRedirect::buildXML(std::stringstream &s) {
	s << "\t<cgi name='" << name << "' cmd='" << cmd << "' ";
	if (args.size() > 0) {
		s << "arg='";
		for (size_t i = 0; i < args.size(); i++) {
			s << args[i];
			if (i != args.size() - 1) {
				s << " ";
			}
		}
		s << "' ";
	}
	if (envs.size() > 0) {
		s << "env='";
		for (size_t i = 0; i < envs.size(); i++) {
			s << envs[i];
			if (i != envs.size() - 1) {
				s << split_char;
			}
		}
		s << "' ";
		if (split_char != '|') {
			s << "env_split='" << split_char << "' ";
		}
	}
	s << "/>\n";
}
#endif