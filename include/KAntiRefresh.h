//{{ent
/*
 * KAntiRefresh.h
 *
 *  Created on: 2010-5-31
 *      Author: keengo
 *
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

#ifndef KANTIREFRESH_H_
#define KANTIREFRESH_H_
#include <list>
#include "global.h"
#include "KMutex.h"
#include "KHttpRequest.h"
#include "KTargetRate.h"
#include "KWriteBack.h"
#include "KAutoBuffer.h"

#define CC_BUFFER_STRING    0
#define CC_BUFFER_URL       1
#define CC_BUFFER_MURL      2
#define CC_BUFFER_REVERT    3
#define CC_BUFFER_SB        4
#define CC_BUFFER_SC_KEY    5
#define CC_BUFFER_PROXY     6
#ifdef ENABLE_FATBOY
void flush_net_request(time_t now_time);
#endif
#endif /* KANTIREFRESH_H_ */
//}}
