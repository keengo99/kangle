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
#include "KDefer.h"

using namespace std;
KUrlValue::KUrlValue() {
}

KUrlValue::~KUrlValue() {
	for (auto it = subs.begin(); it != subs.end(); it++) {
		delete (*it).second;
	}
}
const KString&KUrlValue::get(const KString &name) const {
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
const KString &KUrlValue::get(const char *name) const
{
	return attribute[name];
}
bool KUrlValue::get(const KString name, KString&value) const {
	auto it = attribute.find(name);
	if (it == attribute.end()) {
		return false;
	}
	value = (*it).second;
	return true;
}

void KUrlValue::get(std::map<KString, KString> &values) const {
	values = attribute;
}
bool KUrlValue::parse(char* buf, size_t len) {
	if (buf == NULL) {
		return false;
	}
	KUrlValue* cur_uv = this;
	std::stack<KUrlValue*> uvs;
	char* name;
	char* value;
	char* tmp;
	char* msg;
	char* ptr;
	for (size_t i = 0; i < len; i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			break;
		}
	}
	tmp = buf;
	char split = '=';
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		if (split == '=') {
			name = msg;
			split = '&';
		} else {
			url_decode(msg);
			value = msg;
			split = '=';
			url_decode(name);
			if (strcmp(name,"begin_sub_form")==0) {
				auto uv = new KUrlValue;
				cur_uv->subs.emplace(value, uv);
				uvs.push(cur_uv);
				cur_uv = uv;
			} else if (strcmp(name,"end_sub_form")==0) {
				if (uvs.empty()) {
					return false;
				}
				cur_uv = uvs.top();
				uvs.pop();
			} else {
				cur_uv->put(name, value);
			}
		}

	}
	return true;
}
bool KUrlValue::parse(const char *param) {
	if (param == NULL) {
		return false;
	}
	char *buf = xstrdup(param);
	defer(free(buf));
	return parse(buf,strlen(buf));
}
void KUrlValue::put(const KString& name, const KString& value) {
	attribute.emplace(name, value);
}