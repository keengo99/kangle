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
#ifndef KHTMLSUPPORTEXCEPTION_H_
#define KHTMLSUPPORTEXCEPTION_H_
#include <exception>
#include<string>
/**
 * KHtmlSupport 异常类，发生异常时抛出
 */
#if 0
class KHtmlSupportException : public std::exception
{
public:
	KHtmlSupportException();
	virtual ~KHtmlSupportException() throw();
	const char *what() const throw();
	void setMsg(const char *msg);
private:
	std::string msg;
};
#endif
#endif /*KHTMLSUPPORTEXCEPTION_H_*/
