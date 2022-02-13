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
#ifndef KJUMP_H_
#define KJUMP_H_
#include<string>
#include "KXmlSupport.h"
#include "KMutex.h"
#include "KCountable.h"
class KJump : public KXmlSupport,public KCountableEx
{
public:
	std::string name;
	KJump(){}
	virtual ~KJump();
};
#endif /*KJUMP_H_*/
