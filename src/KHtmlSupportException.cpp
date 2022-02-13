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
#include "KHtmlSupportException.h"
#include "kmalloc.h"
#if 0
KHtmlSupportException::KHtmlSupportException()
{
}

KHtmlSupportException::~KHtmlSupportException() throw()
{
}
const char *KHtmlSupportException::what() const throw()
{
	return msg.c_str();
}
void KHtmlSupportException::setMsg(const char *msg)
{
	this->msg=msg;
}
#endif