/*
 * KAttribute.cpp
 *
 *  Created on: 2010-8-8
 *      Author: keengo
 */

#include "KAttribute.h"
#if 0
using namespace std;

KAttribute::KAttribute() {
}

KAttribute::~KAttribute() {
	std::map<char *, char *, attrp>::iterator it;
	for (it = atts.begin(); it != atts.end(); it++) {
		xfree((*it).first);
		xfree((*it).second);
	}
}
const char *KAttribute::getValue(const char *name) {
	std::map<char *, char *, attrp>::iterator it;
	it = atts.find((char *) name);
	if (it == atts.end()) {
		return NULL;
	}
	return (*it).second;
}
void KAttribute::add(const char *name, const char *value) {
	add(xstrdup(name), xstrdup(value));
}
void KAttribute::add(char *name, char *value) {
	del(name);
	atts.insert(pair<char *, char *> (name, value));
}
void KAttribute::del(const char *name) {
	std::map<char *, char *, attrp>::iterator it;
	it = atts.find((char *) name);
	if (it != atts.end()) {
		xfree((*it).first);
		xfree((*it).second);
		atts.erase(it);
	}
}
#endif