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
#include <stdlib.h>
#include <vector>
#include "KLangParser.h"
#include "KXml.h"
#include "do_config.h"
#include "utils.h"
#include<sstream>
#include "kmalloc.h"
using namespace std;
KLangParser::KLangParser() {
}

KLangParser::~KLangParser() {
}
bool KLangParser::startElement(KXmlContext *context,
		std::map<std::string,std::string> &attribute) {
	if (context->qName == "include") {
		KXml xml;
		KLangParser parser;
		xml.setEvent(&parser);
		xml.parseFile(conf.path+attribute["file"]);
		map<string,string>::iterator it;
		for (it=parser.langs.begin(); it!=parser.langs.end(); it++) {
			langs.insert(*it);
		}
	}
	if (context->qName == "lang") {
		map<string,string>::iterator it;
		for (it=attribute.begin(); it!=attribute.end(); it++) {
			langs.insert(*it);
		}
	}
	return true;
}
bool KLangParser::startCharacter(KXmlContext *context, char *character,
		int len) {
	string value=replace(character, langs, "${", "}");
	langs[context->qName]=value;
	return true;
}
