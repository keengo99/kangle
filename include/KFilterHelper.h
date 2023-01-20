/*
 * KFilterHelper.h
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


#ifndef KFILTERHELPER_H_
#define KFILTERHELPER_H_
#include <assert.h>
#include "KContentMark.h"
#include "kmalloc.h"
#if 0
class KFilterHelper {
public:
	KFilterHelper(KContentMark * filter, int jump) {
		filter->addRef();
		this->filter = filter;
		lastResult = FILTER_NOMATCH;
		this->jump = jump;
	}
	~KFilterHelper() {
		filter->release();
	}
	int check(const char *buf, int len, const char *charset, KFilterKey **keys) {
		assert(filter);
		lastResult = filter->check(buf, len, charset, keys);
		return lastResult;
	}
	KContentMark *filter;
	KFilterHelper *next;
	int jump;
	int lastResult;
};
#endif
#endif /* KFILTERHELPER_H_ */
