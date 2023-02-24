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
#ifndef KLOGPARSER_H_
#define KLOGPARSER_H_
#include <time.h>
#include "KXmlEvent.h"
#include "KVirtualHost.h"
#include "KAccess.h"
#include "KXmlAttribute.h"
#ifndef HTTP_PROXY

class KHttpServerParser : public KXmlEvent {
public:
	KHttpServerParser();
	virtual ~KHttpServerParser();
	bool parse(std::string file);
	bool parseString(const char *buf);
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
	void startXml(const std::string &encoding) override;
	void endXml(bool result) override;
	static bool buildVirtualHost(KAttributeHelper *ah,KVirtualHost *virtualHost,KBaseVirtualHost *gvh,KTempleteVirtualHost *tm,KVirtualHost *ov = NULL);
	static KVirtualHost *buildVirtualHost(KXmlAttribute&attribute,KBaseVirtualHost *gvh,KTempleteVirtualHost *tm,KVirtualHost *ov);
	static KVirtualHost *buildVirtualHost(KAttributeHelper *ah,KBaseVirtualHost *gvh,KTempleteVirtualHost *tm,KVirtualHost *ov = NULL);
	static bool buildBaseVirtualHost(KXmlAttribute &attribute,KBaseVirtualHost *vh);
	KAccess *kaccess[2];
private:
	bool ParseSsl(KVirtualHostManage *vhm, std::map<std::string, std::string> &attribute);
	KVirtualHost *virtualHost;
	KTempleteVirtualHost *cur_tvh;
};
#endif
#endif /*KLOGPARSER_H_*/
