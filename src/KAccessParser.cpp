/*
 * KAccessParser.cpp
 *
 *  Created on: 2010-4-24
 *      Author: keengo
 *
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
#include <stdlib.h>
#include "KAccessParser.h"
#include "KXml.h"
#include "kmalloc.h"

KAccessParser::KAccessParser() {

}

KAccessParser::~KAccessParser() {

}

bool KAccessParser::startElement(KXmlContext *context) {
	for (int i = 0; i < 2; i++) {
		access[i].startElement(context);
	}
	return true;
}
bool KAccessParser::startCharacter(KXmlContext *context, char *character,
		int len) {
	for (int i = 0; i < 2; i++) {
		access[i].startCharacter(context, character, len);
	}
	return true;
}
bool KAccessParser::endElement(KXmlContext *context) {
	for (int i = 0; i < 2; i++) {
		access[i].endElement(context);
	}
	return true;
}
bool KAccessParser::parseString(const char *str,KAccess *access)
{
	this->access = access;
	KXml xmlParser;
	xmlParser.setEvent(this);
	bool result=false;
	try {
		result=xmlParser.parseString(str);
	} catch(KXmlException &e) {
		fprintf(stderr,"%s\n",e.what());
		return false;
	}
	return result;
}
bool KAccessParser::parseFile(std::string file,KAccess *access)
{
	this->access = access;
	KXml xmlParser;
	xmlParser.setEvent(this);
	bool result=false;
	try {
		result=xmlParser.parseFile(file);
	} catch(KXmlException &e) {
		fprintf(stderr, "%s\n", e.what());
		return false;
	}
	return result;
}

void KAccessParser::startXml(const std::string &encoding)
{
	for (int i = 0; i < 2; i++) {
		access[i].startXml(encoding);
	}
}
void KAccessParser::endXml(bool success)
{
	for (int i = 0; i < 2; i++) {
		access[i].endXml(success);
	}
}

