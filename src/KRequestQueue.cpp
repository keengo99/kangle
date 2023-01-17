#include <string.h>
#include <string>
#include "KRequestQueue.h"
#include "kthread.h"
#include "kselectable.h"
#include "http.h"
#include "kselector.h"
#include "kmalloc.h"
#ifdef ENABLE_REQUEST_QUEUE
KRequestQueue globalRequestQueue;

KRequestQueue::KRequestQueue()
{
	lock = kfiber_mutex_init();
	max_queue = 0;
}
KRequestQueue::~KRequestQueue()
{
	kfiber_mutex_destroy(lock);
}
void KRequestQueue::set(unsigned max_worker,unsigned max_queue)
{
	this->max_worker = max_worker;
	this->max_queue = max_queue;
	kfiber_mutex_set_limit(lock, (int)max_worker);
}
void KRequestQueue::stop()
{
	kfiber_mutex_unlock(lock);
}
bool KRequestQueue::start()
{
	return kfiber_mutex_try_lock(lock, max_queue) == 0;
}
#endif
