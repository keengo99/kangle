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
#ifndef kmutex_h_lsdkjfs0d9f8sdf9
#define kmutex_h_lsdkjfs0d9f8sdf9
#include "global.h"
#ifndef _WIN32
#include<pthread.h>
#else
#include "kforwin32.h"
//#define DEAD_LOCK   1
#endif
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "ksync.h"
#ifndef DEAD_LOCK
#define Lock Lock2
#define Unlock	Unlock2
#else
#include "trace.h"
#ifdef _WIN32
#define Lock() Lock3(__FILE__,__LINE__)
#endif
#endif

class KMutex {
public:
	KMutex() {
		pthread_mutex_init(&mutex, NULL);
#ifdef DEAD_LOCK
#ifdef _WIN32
		file = NULL;
		line = 0;
#else
		where_locked[0] = 0;
		locked = false;
		lockTime = 0;	
#endif
#endif
	}
#ifndef _WIN32
	KMutex(int type) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, type);
		pthread_mutex_init(&mutex, &attr);
	}
#endif
	~KMutex() {
		pthread_mutex_destroy(&mutex);

	}
#ifdef DEAD_LOCK
#ifdef _WIN32
	int Lock3(const char *file,int line)
	{
		int ret = Lock2();
		assert(WAIT_OBJECT_0==ret);
		this->file = file;
		this->line = line;
		tid = pthread_self();
		return ret;

	}
	int Unlock(){
		file = NULL;
		line = 0;
		int ret = Unlock2();
		assert(ret>0);
		return ret;
	}
#else
	int Lock() {
		/*
		if(locked){
			if(time(NULL)-lockTime>5){
				abort();
			}
		}
		*/
		int ret = Lock2();
		lockTime = time(NULL);
		locked = true;
		generate_traceback(where_locked);
		return ret;
	}
	int Unlock() {
		locked = false;
		int ret = Unlock2();
		return ret;

	}
#endif
#endif
	int Lock2() {
		return pthread_mutex_lock(&mutex);
	}
	int Unlock2() {
		return pthread_mutex_unlock(&mutex);
	}
private:
#ifdef DEAD_LOCK
#ifndef _WIN32
	TRACEBACK where_locked;
	time_t lockTime;
	bool locked;
#else
	const char *file;
	int line;
	pthread_t tid;
#endif
#endif
	pthread_mutex_t mutex;
};

#ifdef _WIN32
#define KReMutex KMutex
#else
class KReMutex: public KMutex
{
public:
	KReMutex() : KMutex(PTHREAD_MUTEX_RECURSIVE)
	{
	};
};
#endif
class KLocker
{
public:
	KLocker(KMutex *lock)
	{
		lock->Lock();
		this->lock = lock;
	}
	~KLocker()
	{
		lock->Unlock();
	}
private:
	KMutex *lock;
};
#endif

