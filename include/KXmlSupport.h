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
#ifndef KXMLSUPPORT_H_
#define KXMLSUPPORT_H_
#include<map>
#include<string>
#include<sstream>
#include "KXmlEvent.h"
#include "KXmlDocument.h"
#include "KStream.h"
#if 0
class KXmlParser : public KXmlEvent {
public:
	virtual ~KXmlParser()
	{

	}
};

class KXmlBuilder {
public:
	virtual ~KXmlBuilder()
	{

	}
    virtual void buildXML(std::stringstream& s, int flag) {
        buildXML(s);
    }
protected:
    virtual void buildXML(std::stringstream& s) {
    }

};
class KXmlSupport: public KXmlParser, public KXmlBuilder {
public:
	KXmlSupport();
	virtual ~KXmlSupport();
};
#endif
#endif /*KXMLSUPPORT_H_*/
