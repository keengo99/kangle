#ifndef KASYNCWORKER_H
#define KASYNCWORKER_H
#include "KMutex.h"
#include "KCondWait.h"
#include "KCountable.h"
#include "ksapi.h"
#if 0
//异步工作
/**
* 异步回调函数
*/
typedef KTHREAD_FUNCTION (* asyncWorkerCallBack)(void *data,int msec);


struct KAsyncParam {
	void *data;
	KCondWait *wait;
	INT64 startTime;
	asyncWorkerCallBack callBack;
	KAsyncParam *next;
};
bool thread_start_worker(void *param, asyncWorkerCallBack callback);
//异步工作类
class KAsyncWorker : public KCountableEx
{
public:
	KAsyncWorker()
	{
		maxWorker = 1;
		worker = 0;
		queue = 0;
		maxQueue = 0;
		head = NULL;
		last = NULL;
	}
	KAsyncWorker(int maxWorker,int maxQueue)
	{
		this->maxWorker = maxWorker;
		this->maxQueue = maxQueue;
		worker = 0;
		queue = 0;
		if (this->maxWorker<=0) {
			this->maxWorker = 1;
		}
		if (this->maxQueue > 0 && this->maxQueue < this->maxWorker) {
			this->maxQueue = this->maxWorker;
		}
		head = NULL;
		last = NULL;
	}
	bool tryStart(void *data,asyncWorkerCallBack callBack,bool high=false);
	void start(void *data, asyncWorkerCallBack callBack);
	void workThread();
	bool isEmpty()
	{
		lock.Lock();
		bool result = (head==NULL);
		lock.Unlock();
		return result;
	}
	int getQueue()
	{
		lock.Lock();
		int ret = queue;
		lock.Unlock();
		return ret;
	}
	int getWorker()
	{
		lock.Lock();
		int ret = worker;
		lock.Unlock();
		return ret;
	}
	int get_max_queue()
	{
		return maxQueue;
	}
	int get_max_worker()
	{
		return maxWorker;
	}
	void setWorker(int maxWorker,int maxQueue)
	{
		this->maxWorker = maxWorker;
		this->maxQueue = maxQueue;
		if (this->maxWorker<=0) {
			this->maxWorker = 1;
		}
		if (this->maxQueue > 0 && this->maxQueue < this->maxWorker) {
			this->maxQueue = this->maxWorker;
		}
	}
	~KAsyncWorker()
	{
		assert(head == NULL);
		while (head) {
			last = head->next;
			delete head;
			head = last;
		}
	}
private:
	void add(KAsyncParam *rq, bool high);
	KMutex lock;
	KAsyncParam *head;
	KAsyncParam *last;
	int maxWorker;
	int maxQueue;
	int worker;
	int queue;
};
#endif
#endif
