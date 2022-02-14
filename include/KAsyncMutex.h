#ifndef KASYNCMUTEX_H
#define KASYNCMUTEX_H
#include "KMutex.h"
enum Lock_result
{
	Lock_success,
	Lock_failed,
};
typedef void (*lockCallBack)(void *param,Lock_result ret);
struct KLockStack
{
	lockCallBack callBack;
	void *param;
	KLockStack *next;
};
/**
* async lock non-block
*/
class KAsyncMutex
{
public:
	KAsyncMutex()
	{
		refs = 0;
		head = NULL;
		last = NULL;
		
	}
	~KAsyncMutex()
	{
		assert(head==NULL);
		//防御性编程，正则情况下head为空，要全部处理
		while (head) {
			head->callBack(head->param,Lock_failed);
			last = head->next;
			delete head;
			head = last;
		}
			
	}
	void lock(lockCallBack callBack,void *param,bool high=false)
	{
		bool direct = false;
		mutex.Lock();
		if (refs==0) {
			assert(head==NULL);
			direct = true;
		} else {
			KLockStack *stack = new KLockStack;
			stack->callBack = callBack;
			stack->param = param;
			if (high) {
				stack->next = head;
				if (last==NULL) {
					last = stack;
				}
				head = stack;
			} else {
				stack->next = NULL;
				if (last) {
					last->next = stack;
					assert(head);
				} else {
					head = stack;
				}
				last = stack;
			}
		}
		refs++;
		mutex.Unlock();
		if (direct) {
			callBack(param,Lock_success);
		}
	}
	void unlock()
	{
		mutex.Lock();
		KLockStack *stack = head;
		if (head) {
			head = head->next;
			if (head==NULL) {
				last = NULL;
			}
		}
		refs--;
		assert(refs>=0);
		mutex.Unlock();
		if (stack) {
			stack->callBack(stack->param,Lock_success);
			delete stack;
		}
	}
	int getRefs()
	{
		mutex.Lock();
		int ret = refs;
		mutex.Unlock();
		return ret;
	}
	inline int getRefsFast()
	{
		return refs;
	}
private:
	KMutex mutex;
	int refs;
	KLockStack *head;
	KLockStack *last;
};
#endif

