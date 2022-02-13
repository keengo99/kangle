/*
 * KCgi.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "KCgi.h"
#ifndef _WIN32
extern char** environ;

KCgi::KCgi() {

}

KCgi::~KCgi() {

}
bool KCgi::redirectIO(int rin, int rout, int rerr) {
	if (rin != 0 && rin > 0) {
		dup2(rin, 0);
	}
	if (rout != 1 && rout > 0) {
		dup2(rout, 1);
	}
	if (rerr != 2 && rerr > 0) {
		dup2(rerr, 2);
	}
	return true;
}
bool KCgi::run(const char *cmd, char ** arg, KCgiEnv *env) {
	//env->regEnv();
	if (cmdModel) {
		//	env->regEnv();
		//fprintf(stderr,"run cmd[%s]\n",cmd);
		execvp(cmd, arg);
	} else {
		execve(cmd, arg, env->dump_env());
	}
	fprintf(stdout, "Status: 500\r\n\r\ncann't run cmd=%s,errno=%d %s\n", cmd,
			errno, strerror(errno));
	_exit(0);
}
#endif
