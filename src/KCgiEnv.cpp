/*
 * KCgiEnv.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#include <string.h>
#include <stdio.h>
#include "kforwin32.h"
#include "KCgiEnv.h"
#include "kmalloc.h"
using namespace std;
KCgiEnv::KCgiEnv() {
	env = NULL;
}

KCgiEnv::~KCgiEnv() {
	list<char *>::iterator it;
	for (it = m_env.begin(); it != m_env.end(); it++) {
		xfree(*it);
	}
	if(env){
		xfree(env);
	}
}
bool KCgiEnv::add_env(const char* attr, size_t attr_len, const char* val, size_t val_len)
{
	int len = (int)(attr_len + val_len + 1);
	char* str = (char*)xmalloc(len + 1);
	if (str == NULL) {
		return false;
	}	
	kgl_memcpy(str, attr, attr_len);
	kgl_memcpy(str + attr_len, _KS("="));
	kgl_memcpy(str + attr_len + 1, val, val_len);
	str[len] = '\0';
	m_env.push_back(str);
	return true;
}
bool KCgiEnv::addEnv(const char *attr, const char *val) {
	return add_env(attr, strlen(attr), val, strlen(val));
}
bool KCgiEnv::addEnv(const char *env) {
	m_env.push_back(xstrdup(env));
	return true;
}
char **KCgiEnv::dump_env() {
	return env;
}
bool KCgiEnv::addEnvEnd() 
{
	kassert(env==NULL);
	if(env){
		xfree(env);
	}
	env = (char **) xmalloc(m_env.size() * (sizeof(char *) + 1));
	list<char *>::iterator it;
	int i;
	for (i = 0, it = m_env.begin(); it != m_env.end(); it++, i++) {
		env[i] = (*it);
	}
	env[i] = NULL;
	return true;
}

