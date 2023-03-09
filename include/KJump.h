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
#include "katom.h"
#include "KSharedObj.h"

class KJump
{
public:
	KJump() {
		ref = 1;
	}
	KJump(const std::string &a): name(a) {
		ref = 1;
	}
	KJump(std::string&& a) noexcept : name(a) {
		ref = 1;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	KJump* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	uint32_t get_ref() {
		return katom_get((void*)&ref);
	}
	std::string name;
protected:
	virtual ~KJump();
private:
	volatile uint32_t ref;
};
using KSafeJump = KSharedObj<KJump>;
#endif /*KJUMP_H_*/
