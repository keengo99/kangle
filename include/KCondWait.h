#ifndef KCONDWAIT_H
#define KCONDWAIT_H
#include "kforwin32.h"
/**
条件变量
autoreset = true,自动重置变量，适应同时一个等待，多次使用的场景
autoreset = false,不自动重置变量，适应多个同时等待，一次使用的场景
*/
#ifdef _WIN32
class KCondWait
{
public:
	KCondWait(bool autoreset=true)
	{
		h = CreateEvent(NULL,(autoreset?FALSE:TRUE),FALSE,NULL);
	}
	~KCondWait()
	{
		CloseHandle(h);
	}
	bool tryWait(int msec)
	{
		return WAIT_OBJECT_0==WaitForSingleObject(h,msec);
	}
	void wait()
	{
		WaitForSingleObject(h,INFINITE);
	}
	void notice()
	{
		SetEvent(h);
	}
private:
	HANDLE h;
};
#else
#include <pthread.h>
#include <sys/time.h>
class KCondWait
{
public:
	KCondWait(bool autoreset=true)
	{
		this->autoreset = autoreset;
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond,NULL);
		ev = false;
	}
	~KCondWait()
	{
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
	}
	bool tryWait(int msec)
	{
		pthread_mutex_lock(&mutex);
		if (ev) {
			ev = false;
			pthread_mutex_unlock(&mutex);
			return true;
		}
		struct timeval now;

		  /* 获取当前时间 */
		gettimeofday(&now,NULL);
		timespec ts;
		ts.tv_sec = now.tv_sec + msec/1000;
		ts.tv_nsec  = now.tv_usec * 1000 - ts.tv_sec * 1000000 +  msec * 1000;
		bool result = (0==pthread_cond_timedwait(&cond,&mutex,&ts));
		if (result && autoreset) {
			ev = false;
		}
		pthread_mutex_unlock(&mutex);
		return result;
	}
	void wait()
	{
		pthread_mutex_lock(&mutex);
		if (ev) {
			ev = false;
			pthread_mutex_unlock(&mutex);
			return;
		}
		pthread_cond_wait(&cond,&mutex);
		if (autoreset) {
			ev = false;
		}
		pthread_mutex_unlock(&mutex);
	}
	void notice()
	{
		pthread_mutex_lock(&mutex);
		ev = true;
		if (autoreset) {
			pthread_cond_signal(&cond);
		} else {
			pthread_cond_broadcast(&cond);
		}
		pthread_mutex_unlock(&mutex);
	}
private:
	bool ev;
	bool autoreset;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};
#endif
#endif
