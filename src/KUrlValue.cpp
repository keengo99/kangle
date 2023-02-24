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
#include <string.h>
#include "utils.h"
#include "http.h"
#include "KUrlValue.h"
#include "KUrlParser.h"
#include "kmalloc.h"

using namespace std;
KUrlValue::KUrlValue() {
	next = NULL;
	sub = NULL;
	flag = false;
}

KUrlValue::~KUrlValue() {
	map<string, KUrlValue *>::iterator it;
	for (it = subs.begin(); it != subs.end(); it++) {
		delete (*it).second;
	}
	if (next) {
		delete next;
	}
}
KUrlValue *KUrlValue::getNextSub(const std::string &name, int &index)  const {
	auto it = subs.find(name);
	if (it == subs.end())
		return NULL;
	index = 0;
	KUrlValue *last = (*it).second;
	while (last->next && last->flag) {
		last = last->next;
		index++;
	}
	if (last->flag) {
		return NULL;
	}
	last->flag = true;
	return last;
}
KUrlValue *KUrlValue::getSub(const std::string &name, int index) const {
	auto it = subs.find(name);
	if (it == subs.begin())
		return NULL;
	KUrlValue *last = (*it).second;
	while (last->next && index > 0) {
		last = last->next;
		index--;
	}
	if (index > 0) {
		return NULL;
	}
	return last;
}
const std::string &KUrlValue::get(const std::string &name) const {
	return attribute[name];
}
const char *KUrlValue::getx(const char *name) const
{
	auto it = attribute.find(name);
	if (it == attribute.end()) {
		return NULL;
	}
	return (*it).second.c_str();
}
const std::string &KUrlValue::get(const char *name) const
{
	return attribute[name];
}
bool KUrlValue::get(const std::string name, std::string &value) const {
	auto it = attribute.find(name);
	if (it == attribute.end()) {
		return false;
	}
	value = (*it).second;
	return true;
}

void KUrlValue::get(std::map<std::string, std::string> &values) const {
	values = attribute;
}

void KUrlValue::add(KUrlValue *subform) {
	if (next == NULL) {
		next = subform;
		return;
	}
	KUrlValue *last = next;
	while (last->next) {
		last = last->next;
	}
	last->next = subform;
}
bool KUrlValue::parse(const char *param) {
	if (param == NULL) {
		return false;
	}
	char *buf = xstrdup(param);
	char *name;
	char *value;
	char *tmp;
	char *msg;
	char *ptr;
	for (size_t i = 0; i < strlen(buf); i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			break;
		}
	}
	//	url_unencode(param);
	//printf("param=%s.\n",param);
	tmp = buf;
	char split = '=';
	//	strcpy(split,"=");
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		if (split == '=') {
			name = msg;
			split = '&';
		} else {//strtok_r(msg,"=",&ptr2);
			url_decode(msg);
			value = msg;//strtok_r(NULL,"=",&ptr2);

			split = '=';
			for (size_t i = 0; i < strlen(name); i++) {
				name[i] = tolower(name[i]);
			}
			url_decode(name);
			//urlParam.insert(pair<string, string> (name, value));
			add(name, value);
		}

	}
	xfree(buf);
	return true;
}
void KUrlValue::put(const std::string& name, const std::string& value) {
	attribute.emplace(name, value);
}
bool KUrlValue::add(const std::string& name, const std::string& value) {
	if (sub) {
		if (sub->add(name, value))
			return true;
	}
	if (name == "begin_sub_form") {
		sub = new KUrlValue;
		auto it = subs.find(value);
		if (it == subs.end()) {
			subs.emplace(value, sub);
		} else {
			(*it).second->add(sub);
		}
		return true;
	}
	if (name == "end_sub_form") {
		if (sub == NULL) {
			return false;
		}
		sub = NULL;
		return true;
	}
	auto it2 =  attribute.find(name);
	if(it2==attribute.end()){
		attribute.emplace(name, value);
	}else{
		stringstream s;
		s << (*it2).second << ", " << value;
		attribute.emplace(name, s.str());
	}
	return true;
}
