#include "KAsyncWorker.h"
#include "kthread.h"
#if 0
KTHREAD_FUNCTION kasyncWorkerThread(void *param)
{
	KAsyncWorker *worker = (KAsyncWorker *)param;
	worker->workThread();
	KTHREAD_RETURN;
}
struct thread_start_worker_param {
	void *param;
	asyncWorkerCallBack callback;
};
KTHREAD_FUNCTION thread_start_worker_thread(void *param)
{
	thread_start_worker_param *p = (thread_start_worker_param *)param;
	p->callback(p->param, 0);
	delete p;
	KTHREAD_RETURN;
}
bool thread_start_worker(void *param, asyncWorkerCallBack callback)
{
	thread_start_worker_param *p = new thread_start_worker_param;
	p->param = param;
	p->callback = callback;
	if (m_thread.start(p, thread_start_worker_thread)) {
		return true;
	}
	delete p;
	return false;
}
void KAsyncWorker::start(void *data, asyncWorkerCallBack callBack)
{
	KCondWait *wait = NULL;
	KAsyncParam *rq = new KAsyncParam;
	rq->startTime = kgl_current_msec;
	rq->callBack = callBack;
	rq->data = data;
	rq->next = NULL;
	lock.Lock();
	if (maxQueue > 0 && queue >= this->maxQueue) {
		wait = new KCondWait;
	}
	rq->wait = wait;
	add(rq, false);
	lock.Unlock();
	if (wait != NULL) {
		wait->wait();
		delete wait;
	}
}
void KAsyncWorker::add(KAsyncParam *rq, bool high)
{
	queue++;
	if (last == NULL) {
		assert(head == NULL);
		head = rq;
		last = rq;
	} else {
		if (high) {
			//push head
			rq->next = head;
			head = rq;
		} else {
			//push end
			last->next = rq;
			last = rq;
		}
	}
	if (worker >= maxWorker) {
		return;
	}
	worker++;
	addRef();
	if (!m_thread.start(this, kasyncWorkerThread)) {
		worker--;
		release();
	}
	return;
}
bool KAsyncWorker::tryStart(void *data,asyncWorkerCallBack callBack,bool high)
{
	lock.Lock();
	if (maxQueue>0 && queue > this->maxQueue) {
		lock.Unlock();
		return false;
	}
	KAsyncParam *rq = new KAsyncParam;
	rq->startTime = kgl_current_msec;
	rq->callBack = callBack;
	rq->data = data;
	rq->next = NULL;
	rq->wait = NULL;
	add(rq,high);
	lock.Unlock();
	return true;
}
void KAsyncWorker::workThread()
{
	for (;;) {
		KAsyncParam *rq = NULL;
		lock.Lock();
		if (head==NULL) {
			worker--;
			lock.Unlock();
			break;
		}
		queue--;
		rq = head;
		head = head->next;
		if (head==NULL) {
			last = NULL;
		}
		lock.Unlock();
		int msec = (int)(kgl_current_msec - rq->startTime);
		if (msec < 0) {
			msec = 0;
		}
		if (rq->wait) {
			rq->wait->notice();
		}
		rq->callBack(rq->data, msec);
		delete rq;
	}
	release();
}
#endif