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
#ifndef KFileMsg_h_sdfslfjsdf
#define KFileMsg_h_sdfslfjsdf
#include<stdlib.h>
class KFileMsg
{
public:
	KFileMsg();
	~KFileMsg();
	bool operator !();
	char *str;
	int len;
};
#endif
