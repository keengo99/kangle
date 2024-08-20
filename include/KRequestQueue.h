#ifndef KREQUESTQUEUE_H
#define KREQUESTQUEUE_H
#include "KCountable.h"
#include "kthread.h"
#include "kfiber_sync.h"
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue : public KCountableEx
{
public:
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
		
	KRequestQueue()
	{
		lock = kfiber_mutex_init();
		max_queue = 0;
	}
	~KRequestQueue()
	{
		kfiber_mutex_destroy(lock);
	}
	void set(unsigned max_worker,unsigned max_queue)
	{
		this->max_worker = max_worker;
		this->max_queue = max_queue;
		kfiber_mutex_set_limit(lock, (int)max_worker);
	}
	void stop()
	{
		kfiber_mutex_unlock(lock);
	}
	bool start()
	{
		return kfiber_mutex_try_lock(lock, max_queue) == 0;
	}
private:
	kfiber_mutex* lock;
	unsigned max_worker;
	unsigned max_queue;
};
#endif
#endif

