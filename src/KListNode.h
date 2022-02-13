#ifndef KLISTNODE_H
#define KLISTNODE_H
#include <stdlib.h>
class KListNode
{
public:
	KListNode(void);
	virtual ~KListNode(void);
	KListNode *next;
	KListNode *prev;
};
class KList
{
public:
	KList(void);
	virtual ~KList(void);
	virtual void push_back(KListNode *node)
	{
		node->next = NULL;
		node->prev = end;
		if (end == NULL) {
			head = end = node;
		}
		else {
			end->next = node;
			end = node;
		}
	}
	virtual void push_front(KListNode *node)
	{
		node->prev = NULL;
		node->next = head;
		if (head == NULL) {
			head = end = node;
		}
		else {
			head->prev = node;
			head = node;
		}
	}
	virtual void remove(KListNode *node)
	{
		if (node == head) {
			head = head->next;
		}
		if (node == end) {
			end = end->prev;
		}
		if (node->prev) {
			node->prev->next = node->next;
		}
		if (node->next) {
			node->next->prev = node->prev;
		}
		node->next = NULL;
		node->prev = NULL;
		//return node->next;
	}
	KListNode *getHead()
	{
		return head;
	}
	KListNode *getEnd()
	{
		return end;
	}
protected:
	KListNode *head;
	KListNode *end;
};
#endif