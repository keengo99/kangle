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
#ifndef KCOUNTABLE_H_
#define KCOUNTABLE_H_
#include "global.h"
#include "KMutex.h"
#include "KAtomCountable.h"
/*
@deprecated 
请使用KCountableEx
KCountableEx会初始化refs为1.
而KCountable初始化refs为0
*/
class KCountable {
public:
	KCountable() {
		refs = 0;
	}
	virtual ~KCountable() {

	}
	int getRefFast() {
		return refs;
	}
	int getRef() {
		int ret;
		refsLock.Lock();
		ret = refs;
		refsLock.Unlock();
		return ret;
	}
	bool addRefEx(int maxref)
	{
		refsLock.Lock();
		if(refs>=maxref){
			refsLock.Unlock();
			return false;
		}
		refs++;
		refsLock.Unlock();
		return true;
	}
	void addRef() {
		refsLock.Lock();
		refs++;
		refsLock.Unlock();
	}
	void addDoubleRef() {
		refsLock.Lock();
		refs += 2;
		refsLock.Unlock();
	}
	virtual int release() {
		int ret;
		refsLock.Lock();
		refs--;
		ret = refs;
		refsLock.Unlock();
		return ret;
	}
	virtual void destroy() {
		if (release() <= 0) {
			delete this;
		}
	}
	;
protected:
	volatile int refs;
	KMutex refsLock;
};
class KCountableEx {
public:
	KCountableEx() {
		refs = 1;
	}
	int getRefFast() {
		return refs;
	}
	int getRef() {
		int ret;
		refsLock.Lock();
		ret = refs;
		refsLock.Unlock();
		return ret;
	}
	bool addRefEx(int maxref)
	{
		refsLock.Lock();
		if(refs>=maxref){
			refsLock.Unlock();
			return false;
		}
		refs++;
		refsLock.Unlock();
		return true;
	}
	int addRef() {
		int ret;
		refsLock.Lock();
		refs++;
		ret = refs;
		refsLock.Unlock();
		return ret;
	}
	int release() {
		int ret;
		refsLock.Lock();
		refs--;
		ret = refs;
		refsLock.Unlock();
		if(ret<=0){
			delete this;
		}
		return ret;
	}
	inline int destroy()
	{
		return release();
	}
protected:
	virtual ~KCountableEx() {

	}
	volatile int refs;
	KMutex refsLock;
};
#endif /*KCOUNTABLE_H_*/
