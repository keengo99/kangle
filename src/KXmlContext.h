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
#ifndef KXMLCONTEXT_H_
#define KXMLCONTEXT_H_
#include<string>
#include<map>
class KXmlEvent;
class KXml;
class KXmlContext
{
public:
	KXmlContext(KXml *xml);
	virtual ~KXmlContext();
	std::string getParentName();
	KXmlEvent *node;
	KXmlContext *parent;
	std::string qName;
	std::map<std::string,std::string> attribute;
	std::string path;
	int level;
	std::string getPoint();
	void *getData();
private:
	KXml *xml;
};

#endif /*KXMLCONTEXT_H_*/
