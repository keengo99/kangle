#include "KTargetRate.h"
bool KTargetRate::getRate(const char *target,int &request,int &t,bool hit)
{
	KTargetNode *node;
	lock.Lock();
	std::map<char *,KTargetNode *,lessp>::iterator it = nodes.find((char *)target);
	if (it==nodes.end()) {
		if (!hit) {
			lock.Unlock();
			return false;
		}
		node = new KTargetNode();
		node->Init(target);
		nodes.insert(std::pair<char *,KTargetNode *>(node->target,node));
		node->hit(request);
		addList(node);
	} else {
		node = (*it).second;
		if (hit) {
			node->hit(request);
			removeList(node);
			addList(node);
		}
	}
	bool result;
	if (node->count > request) {
		result = true;
		request = node->count;
		t = (int)(node->last->t - node->head->t);
	} else {
		result = false;
	}
	lock.Unlock();
	return result;
}
void KTargetRate::addList(KTargetNode *node)
{
	klist_append(&queue, &node->queue);
}
void KTargetRate::removeList(KTargetNode *node)
{
	klist_remove(&node->queue);
}
void KTargetRate::flush(time_t nowTime,int idleTime)
{
	lock.Lock();
	while (!klist_empty(&queue)) {
		kgl_list *pos = queue.next;
		KTargetNode *node = kgl_list_data(pos, KTargetNode, queue);
		if (nowTime - node->last->t<idleTime) {
			break;
		}
		std::map<char *,KTargetNode *,lessp>::iterator it = nodes.find(node->target);
		kassert(it!=nodes.end());
		kassert((*it).second == node);
		nodes.erase(it);
		klist_remove(pos);
		node->Destroy();
	}
	lock.Unlock();
}
