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
#ifndef KCONFIGPARSER_H_
#define KCONFIGPARSER_H_
#include<string>
#include "KXmlEvent.h"
#include "do_config.h"
#include "kmalloc.h"
#include "KConfigTree.h"

void on_main_event(void *data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_ssl_client_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
void on_admin_event(void* data, kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev);
class KConfigParser : public KXmlEvent{
public:
	void startXml(const std::string &encoding) override;
	void endXml(bool result) override;	
	KConfigParser();
	virtual ~KConfigParser();
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context,
				char *character, int len) override;
	bool endElement(KXmlContext *context) override;
};

#endif /*KCONFIGPARSER_H_*/
