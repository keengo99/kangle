/*
 * KFastcgiUtils.cpp
 *
 *  Created on: 2010-4-21
 *      Author: keengo
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

#include <string>
#include <stdlib.h>
#include "KFastcgiUtils.h"
#include "log.h"
#include "kmalloc.h"
FCGI_BeginRequestRecord fastRequestStart, fastRequestStartKeepAlive;

void initFastcgiData() {
	memset(&fastRequestStart, 0, sizeof(FCGI_BeginRequestRecord));
	fastRequestStart.header.version = 1;
	fastRequestStart.header.type = FCGI_BEGIN_REQUEST;
	fastRequestStart.header.requestIdB0 = 1;
	fastRequestStart.header.contentLengthB0 = sizeof(FCGI_BeginRequestBody);
	fastRequestStart.body.roleB0 = FCGI_RESPONDER;
	fastRequestStartKeepAlive = fastRequestStart;
	fastRequestStartKeepAlive.body.flags |= FCGI_KEEP_CONN;
}




