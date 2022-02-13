#include "KAnticcSession.h"
#include "KMutex.h"
#include "klist.h"
#include "krbtree.h"
static KMutex as_lock;
static KAnticcSession as_list;
static krb_tree *as_tree = NULL;
static int as_max_key = (int)time(NULL);
static int anticc_session_key_cmp(void *key1, void *key2)
{
	return *(int *)key1 - *(int *)key2;
}
void init_anticc_session()
{
	klist_init(&as_list);
	as_tree = rbtree_create();
}
int create_session_number(char *url)
{
	int new_flag = 0;
	KAnticcSession *as = new KAnticcSession(url);
	as->expire_time = time(NULL) + 60;
	as_lock.Lock();
	int key = as_max_key++;
	as->key = key;
	krb_node *node = rbtree_insert(as_tree,&as->key,&new_flag,anticc_session_key_cmp);
	assert(new_flag);
	node->data = as;
	klist_insert(&as_list,as);
	as_lock.Unlock();
	return key;
}
int anticc_session_number(int key)
{
	int number = -1;
	as_lock.Lock();
	krb_node *node = rbtree_find(as_tree,&key,anticc_session_key_cmp);
	if (node) {
		KAnticcSession *as = (KAnticcSession *)node->data;
		number = as->number;
	}
	as_lock.Unlock();
	return number;
}
char *anticc_session_verify(int key,int &number)
{
	char *url = NULL;
	as_lock.Lock();
	krb_node *node = rbtree_find(as_tree,&key,anticc_session_key_cmp);
	if (node==NULL) {
		as_lock.Unlock();
		return url;
	}
	KAnticcSession *as = (KAnticcSession *)node->data;
	number = as->number;
	rbtree_remove(as_tree,node);
	klist_remove(as);
	as_lock.Unlock();	
	url = as->url;
	as->url = NULL;	
	delete as;
	return url;
}
void anticc_session_flush(time_t now_time)
{
	as_lock.Lock();
	for (;;) {
		KAnticcSession *as = klist_head(&as_list);
		if (as==&as_list) {
			break;
		}
		if (as->expire_time>now_time) {
			break;
		}
		krb_node *node = rbtree_find(as_tree,&as->key,anticc_session_key_cmp);
		assert(node);
		rbtree_remove(as_tree,node);
		klist_remove(as);
		delete as;
	}
	as_lock.Unlock();
}
