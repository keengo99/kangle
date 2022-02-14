#ifndef KREQUESTQUEUE_H
#define KREQUESTQUEUE_H
#include "KCountable.h"
#include "KHttpRequest.h"
#include "kthread.h"
#include "kfiber_sync.h"
#ifdef ENABLE_REQUEST_QUEUE

class KRequestQueue : public KCountableEx
{
public:
	KRequestQueue();
	~KRequestQueue();
	bool Lock();
	void Unlock();
	void set(unsigned max_worker,unsigned max_queue);
	unsigned getMaxWorker()
	{
		return max_worker;
	}
	unsigned getMaxQueue()
	{
		return max_queue;
	}
	unsigned getQueueSize()
	{
		return getBusyCount() - getWorkerCount();
	}
	unsigned getWorkerCount()
	{
		return kfiber_mutex_get_worker(lock);
	}
	int getBusyCount()
	{
		return kfiber_mutex_get_count(lock);
	}
	bool Start(KHttpRequest* rq);
private:
	kfiber_mutex* lock;
	unsigned max_worker;
	unsigned max_queue;
};
extern KRequestQueue globalRequestQueue;
#endif
#endif

