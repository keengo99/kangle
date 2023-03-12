/*
 * KAttribute.h
 *
 *  Created on: 2010-8-8
 *      Author: keengo
 */

#ifndef KATTRIBUTE_H_
#define KATTRIBUTE_H_
#include <map>
#include "kmalloc.h"
#include "KXmlAttribute.h"
#if 0
struct attrp
{
	bool operator()(const char* __x, const char* __y) const {
		return strcmp(__x, __y) < 0;
	}
};

class KAttribute {
public:
	KAttribute();
	virtual ~KAttribute();
	const char *getValue(const char *name);
	void add(const char *name, const char *value);
	void add(char *name, char *value);
	void del(const char *name);
	std::map<char *, char *, attrp> atts;
};
#endif
using KAttribute = KXmlAttribute;
#endif /* KATTRIBUTE_H_ */
