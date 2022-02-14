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
#ifndef KLISTENCONFIGPARSER_H
#define KLISTENCONFIGPARSER_H
#include <string>
#include "KXmlEvent.h"
#include "do_config.h"
class KListenConfigParser : public KXmlEvent {
public:
	bool parse(std::string file);
	bool startElement(std::string &context, std::string &qName,
			std::map<std::string,std::string> &attribute);
	bool startCharacter(std::string &context, std::string &qName,
			char *character, int len);
};
class KWorkerConfigParser : public KXmlEvent {
public:
	bool parse(std::string file);
	bool startCharacter(KXmlContext *context, char *character, int len);
};
extern KListenConfigParser listenConfigParser;
extern KWorkerConfigParser worker_config_parser;
#endif
