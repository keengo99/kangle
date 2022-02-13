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
#ifndef KHTMLSUPPORT_H_
#define KHTMLSUPPORT_H_
#include<string>
#include<map>
#include "KHtmlSupportException.h"
#include "KUrlValue.h"
/**
 * HTML±‡“Î÷ß≥÷
 */
class KHtmlSupport {
public:
	KHtmlSupport();
	virtual ~KHtmlSupport();
	virtual std::string getDisplay()=0;
	virtual void editHtml(std::map<std::string, std::string> &attribute, bool html)=0;
};
#endif /*KHTMLSUPPORT_H_*/
