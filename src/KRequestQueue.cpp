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
bool KRequestQueue::Lock()
{
	int result = kfiber_mutex_try_lock(lock, max_queue);
	return result == 0;
}
void KRequestQueue::Unlock()
{
	kfiber_mutex_unlock(lock);
}
bool KRequestQueue::Start(KHttpRequest* rq)
{
	if (rq->queue == NULL) {
		rq->queue = this;
		addRef();
	} else {
		assert(rq->queue == this);
	}
	assert(rq->ctx->queue_handled == 0);
	rq->EnterRequestQueue();
	rq->ctx->queue_handled = Lock();
	rq->LeaveRequestQueue();
	return rq->ctx->queue_handled;
}
#endif
