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
#include "KFileMsg.h"
#include "kmalloc.h"

KFileMsg::KFileMsg()
{
	str=NULL;
	len=0;
}
KFileMsg::~KFileMsg()
{
	if(str!=NULL)
	{
		delete[] str;
	}
}
bool KFileMsg::operator !()
{
	if(str==NULL)
		return false;
	return true;
}