//{{ent
/*
 * KAntiRefresh.cpp
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
#include <vector>
#include "global.h"
#include "KAntiRefresh.h"
#include "KHttpRequest.h"
#include "do_config.h"
#include "KAttackRequestManager.h"
#include "KWhiteList.h"
#include "log.h"
#include "kmd5.h"
#include "lib.h"
#include "KSequence.h"
#include "KLicense.h"
#include "KVirtualHostManage.h"
#include "http.h"
#include "KAnticcSession.h"
#include "kmalloc.h"
#include "KCdnContainer.h"

using namespace std;

void flush_net_request(time_t now_time) {

#ifdef ENABLE_BLACK_LIST
	wlm.flush(now_time,conf.wl_time);
	conf.gvm->globalVh.blackList->flush(conf.bl_time);
#endif
}

