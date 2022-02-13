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
#ifndef KLANGPARSER_H_
#define KLANGPARSER_H_
#include "KXmlEvent.h"
#include<map>
#include<string>
class KLangParser : public KXmlEvent {
public:
	KLangParser();
	virtual ~KLangParser();
	bool startElement(KXmlContext *context,
			std::map<std::string,std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	std::map<std::string,std::string> langs;
};

#endif /*KLANGPARSER_H_*/
