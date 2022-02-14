#ifndef KTARGETRATE_H
#define KTARGETRATE_H
#include <map>
#include <time.h>
#include "extern.h"
#include "klist.h"
#include "KMutex.h"

class KTargetItem
{
public:
	time_t t;
	KTargetItem *next;
};
class KTargetNode
{
public:
	void Init(const char *target)
	{
		this->target = strdup(target);
		count = 0;
		head = NULL;
		last = NULL;
	}
	void Destroy()
	{
		if (target) {
			free(target);
		}
		while (head) {
			last = head->next;
			delete head;
			head = last;
		}
		delete this;
	}
	void hit(int max_request)
	{
		count++;
		KTargetItem *item = new KTargetItem;
		item->t = time(NULL);
		item->next = NULL;
		if (last) {
			last->next = item;
		} else {
			head = item;
		}
		last = item;
		if (count>(max_request+1) && count>1) {
			count--;
			KTargetItem *n = head->next;
			delete head;
			head = n;
		}
	}
	char *target;
	int count;
	KTargetItem *head;
	KTargetItem *last;
	kgl_list queue;
};
class KTargetRate
{
public:
	KTargetRate()
	{
		klist_init(&queue);
	}
	~KTargetRate()
	{
		nodes.clear();
		while (!klist_empty(&queue)) {
			kgl_list *pos = queue.next;
			KTargetNode *node = kgl_list_data(pos, KTargetNode, queue);
			klist_remove(pos);
			node->Destroy();
		}
	}
	bool getRate(const char *target,int &request,int &t,bool hit);
	int getCount()
	{
		lock.Lock();
		int count = (int)nodes.size();
		lock.Unlock();
		return count;
	}
	void flush(time_t nowTime,int idleTime);
private:
	void addList(KTargetNode *node);
	void removeList(KTargetNode *node);
	std::map<char *,KTargetNode *,lessp> nodes;
	kgl_list queue;
	KMutex lock;
};
#endif
