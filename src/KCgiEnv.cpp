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
	if (attr == NULL || val == NULL || attr_len > SIZE_MAX - val_len - 2) {
		return false;
	}
	size_t len = attr_len + val_len + 1;
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
	if (attr == NULL || val == NULL) {
		return false;
	}
	return add_env(attr, strlen(attr), val, strlen(val));
}
bool KCgiEnv::addEnv(const char *env) {
	if (env == NULL) {
		return false;
	}
	char* value = xstrdup(env);
	if (value == NULL) {
		return false;
	}
	m_env.push_back(value);
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
	if (m_env.size() > SIZE_MAX / sizeof(char*) - 1) {
		return false;
	}
	env = (char **)xmalloc((m_env.size() + 1) * sizeof(char*));
	if (env == NULL) {
		return false;
	}
	list<char *>::iterator it;
	int i;
	for (i = 0, it = m_env.begin(); it != m_env.end(); it++, i++) {
		env[i] = (*it);
	}
	env[i] = NULL;
	return true;
}
