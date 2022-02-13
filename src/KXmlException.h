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
#ifndef KXMLEXCEPTION_H_
#define KXMLEXCEPTION_H_
#include<exception>
#include<string>
/*
 * xmlΩ‚Œˆ“Ï≥£¿‡
 */
class KXmlException : public std::exception
{
public:
	KXmlException();
	virtual ~KXmlException() throw();
	const char *what() const throw();
	KXmlException & operator <<(const char *buf);
	KXmlException & operator <<(std::string buf);

	void setMsg(std::string msg);
private:
	std::string msg;
};
#endif /*KXMLEXCEPTION_H_*/
