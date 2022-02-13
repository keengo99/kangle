/*
 * KFilter.cpp
 *
 *  Created on: 2010-5-9
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

#include <string.h>
#include <stdlib.h>
#include "KAccess.h"
#include "KContentMark.h"
#include "KFilterHelper.h"

bool KContentMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
		int &jumpType) {
	KFilterHelper *filterHelper = new KFilterHelper(this, chainJumpType);
	jumpType = JUMP_CONTINUE;
	rq->addFilter(filterHelper);
	return true;
}
