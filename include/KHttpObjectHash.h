#ifndef KHTTPOBJECTHASH_H_
#define KHTTPOBJECTHASH_H_
#include <map>
#include "KHttpObjectNode.h"
#include "KHttpRequest.h"
#include "KMutex.h"
#include "krbtree.h"

typedef void (*objHandler)(KHttpObject *obj,void *param);
inline int cmp_vary_key(KVary *a, KVary *b)
{
	if (a == NULL || a->key == NULL) {
		if (b == NULL || b->key == NULL) {
			return 0;
		} else {
			return 1;
		}
	}
	if (b == NULL || b->key == NULL) {
		return -1;
	}
	return strcasecmp(a->key, b->key);
}
inline int cmp_vary(KVary *a, KVary *b)
{
	if (a == NULL || a->val==NULL) {
		if (b == NULL || b->val==NULL) {
			return 0;
		} else {
			return 1;
		}
	}
	if (b == NULL || b->val==NULL) {
		return -1;
	}
	return strcmp(a->val, b->val);
}
inline int cmp_url_key(KUrlKey *a, KUrlKey *b, bool &match_url)
{
	int ret = a->url->cmp(b->url);
	if (ret != 0) {
		return ret;
	}
	match_url = true;
	return cmp_vary(a->vary, b->vary);
}
inline int cmpn_url_key(KUrlKey *a, KUrlKey *b,int len)
{
	return a->url->cmpn(b->url,len);
}
inline void update_obj_vary_key(KHttpObject *obj, const char *key)
{
	do {
		obj->Dead();
		update_url_vary_key(&obj->uk, key);
		obj = obj->next;
	} while (obj);
}
class KHttpObjectHash {
public:
	friend class KObjectList;
	KHttpObjectHash() {
		mem_size = 0;
		disk_size = 0;
		mem_count = 0;
		disk_count = 0;
		nodes.rb_node = NULL;
	}
	/**
	* 清除指定url的物件，wide指示是否泛匹配,即只匹配前面部分
	*/
	inline int purge(KUrl *url,bool wide,objHandler handle,void *param)
	{
		int count = 0;
		KUrlKey uk;
		uk.url = url;
		uk.vary = NULL;
		lock.Lock();
		if (wide) {	
			int path_len = (int)strlen(url->path);
			krb_node *node = findn(&uk,path_len);
			while (node) {
				count += purgeObject((KHttpObject *)node->data,handle,param);
				node = rb_next(node);
				if (node) {
					KHttpObject *obj = (KHttpObject *)node->data;
					if (cmpn_url_key(&uk,&obj->uk,path_len)!=0) {
						break;
					}
				}
			}
		} else {
			krb_node *node = NULL;
			find(&uk, nodes.rb_node,&node);
			while (node) {
				count += purgeObject((KHttpObject *)node->data, handle, param);
				node = rb_next(node);
				if (node) {
					KHttpObject *obj = (KHttpObject *)node->data;
					if (obj->uk.url->cmp(url) != 0) {
						break;
					}
				}
			}
		}
		lock.Unlock();
		return count;
	}
#ifdef ENABLE_DISK_CACHE
	inline bool FindByFilename(KUrlKey *uk,const char *filename)
	{
		lock.Lock();
		krb_node *node = find(uk, nodes.rb_node,NULL);
		if (node==NULL) {
			lock.Unlock();
			return false;
		}
		KHttpObject *obj = (KHttpObject *)node->data;
		bool result = false;
		while (obj) {
			if (KBIT_TEST(obj->index.flags,FLAG_IN_DISK)) {
				char *file = obj->getFileName();
				if (file) {
					result = (strcmp(file,filename)==0);
					free(file);
					if (result) {
						break;
					}
				}
			}
			obj = obj->next;
		}
		lock.Unlock();
		return result;
	}
#endif
	inline void UpdateVary(KHttpObject *obj, KVary *vary)
	{
		krb_node *match_url_node = NULL;
		lock.Lock();
		krb_node *node = find(&obj->uk, nodes.rb_node, &match_url_node);
		kassert(node);
		kassert(match_url_node);
		InternalUpdateVary(match_url_node, vary);
		lock.Unlock();
	}
	/**
	* 从缓存中查到指定url的物件，gzip,internal指示物件状态
	*/
	inline KHttpObject *get(KHttpRequest *rq) {
		KHttpObject *hit_obj = NULL;
		KHttpObject *obj = NULL;
		KUrlKey uk;
		uk.url = rq->sink->data.url;
		uk.vary = NULL;
		KVary vary;
		lock.Lock();
		krb_node *match_url_node = NULL;
		krb_node *node = find(&uk, nodes.rb_node,&match_url_node);
		if (match_url_node == NULL) {
			lock.Unlock();
			return NULL;
		}
		//printf("match_url_node=[%p]\n", match_url_node);
		obj = (KHttpObject *)match_url_node->data;
		vary.val = obj->BuildVary(rq);
		if (vary.val) {
			uk.vary = &vary;
			//重新查找
			node = find(&uk, match_url_node, NULL);
		}
		if (node==NULL) {
			lock.Unlock();
			return NULL;
		}
		obj = (KHttpObject *)node->data;
		while (obj) {
			if (KBIT_TEST(obj->index.flags,FLAG_DEAD)) {
				obj = obj->next;
				continue;
			}
			if (rq->sink->data.min_obj_verified > 0 && obj->index.last_verified < rq->sink->data.min_obj_verified) {
				//设置了最小验证时间
				if (KBIT_TEST(rq->sink->data.min_obj_verified, 1)>0) {
					//hard
					obj->Dead();
					obj = obj->next;
					continue;
				}
				//soft
				obj->index.last_verified = 0;
			}
			if ((rq->ctx->internal == (KBIT_TEST(obj->index.flags, FLAG_RQ_INTERNAL)>0)) &&
				obj->uk.url->match_accept_encoding(rq->sink->data.raw_url.accept_encoding)) {
				if (!KBIT_TEST(rq->filter_flags, RF_NO_DISK_CACHE) || (obj->data != NULL && obj->data->type == MEMORY_OBJECT)) {
					//hit cache
					if (hit_obj == NULL || obj->uk.url->accept_encoding > hit_obj->uk.url->accept_encoding) {
						hit_obj = obj;
					}
				}
			}
			obj = obj->next;
		}
		if (hit_obj) {
			hit_obj->addRef();
		}
		lock.Unlock();
		return hit_obj;
	}
	void MemObjectSizeChange(KHttpObject *obj,INT64 new_size)
	{
		size_lock.Lock();
		kassert(KBIT_TEST(obj->index.flags, FLAG_IN_MEM));
		//加上新的长度
		mem_size += new_size;
		//减掉旧的长度
		mem_size -= obj->index.content_length;
		size_lock.Unlock();
		obj->index.content_length = new_size;
	}
	/**
	* 增加内存大小
	*/
	void IncMemObjectSize(KHttpObject *obj)
	{		
		size_lock.Lock();
		mem_size += obj->GetMemorySize();
		mem_count++;
		size_lock.Unlock();
	}
	/**
	* 减少内存大小
	*/
	void DecMemObjectSize(KHttpObject *obj)
	{	
		size_lock.Lock();
		mem_size -= obj->GetMemorySize();
		mem_count--;
		kassert(mem_size > 0 || (mem_size == 0 && mem_count == 0));
		size_lock.Unlock();
	}
	/**
	* 增加磁盘大小
	*/
	void IncDiskObjectSize(KHttpObject *obj)
	{			
		size_lock.Lock();
		disk_size += obj->GetDiskSize();
		disk_count++;
		size_lock.Unlock();
	}
	/**
	* 减少磁盘大小
	*/
	void DecDiskObjectSize(KHttpObject *obj)
	{
		size_lock.Lock();
		disk_size -= obj->GetDiskSize();
		disk_count--;
		kassert(disk_size > 0 || (disk_size == 0 && disk_count == 0));
		size_lock.Unlock();
	}	
	/**
	* 得到缓存大小
	*/
	void getSize(INT64 &cacheSize, INT64 &diskSize,int &mem_count,int &disk_count) {
		size_lock.Lock();
		cacheSize += mem_size;
		diskSize += disk_size;
		mem_count += this->mem_count;
		disk_count += this->disk_count;
		size_lock.Unlock();
	}
	bool put(KHttpObject *obj) {
		//	assert(rq->sink->data.url->host && rq->sink->data.url->path);
		//	assert(obj && obj->url == &rq->sink->data.url);
		assert(obj->refs==1);
		//此处可确保obj，不会被其它引用，所以不用加锁
		obj->refs++;
		if (!KBIT_TEST(obj->index.flags,FLAG_URL_FREE)) {
			KUrl *url = obj->uk.url->clone();
			obj->uk.url = url;
			KBIT_SET(obj->index.flags,FLAG_URL_FREE);
		}
		//kassert(obj->h == id);
		kassert(obj->uk.url->host && obj->uk.url->path);
		lock.Lock();
		insert(obj);
		lock.Unlock();
		size_lock.Lock();
		obj->CountSize(mem_size,disk_size,mem_count,disk_count);
		size_lock.Unlock();
		return true;
	}
	//u_short id;
private:
	KMutex lock;
	KMutex size_lock; /* lock to change size		*/
	INT64 mem_size; /* size of objects in this hash */
	INT64 disk_size;
	int mem_count;
	int disk_count;
	void InternalUpdateUrlVary(KUrl *url, krb_node *node, KVary *vary, struct krb_node *(*rb_iterator)(const struct krb_node *))
	{
		for (;;) {
			node = rb_iterator(node);
			if (node == NULL) {
				break;
			}
			KHttpObject *obj = (KHttpObject *)node->data;
			if (obj->uk.url->cmp(url) != 0) {
				break;
			}
			if (cmp_vary_key(obj->uk.vary, vary) != 0) {
				update_obj_vary_key(obj, vary ? vary->key : NULL);
			}			
		}
	}
	void InternalUpdateVary(krb_node *node, KVary *vary)
	{
		KHttpObject *obj = (KHttpObject *)node->data;
		KUrl *url = obj->uk.url;
		update_obj_vary_key(obj, vary ? vary->key : NULL);
		InternalUpdateUrlVary(url, node, vary, rb_next);
		InternalUpdateUrlVary(url, node, vary, rb_prev);
	}
	/**
	* 删除指定物件,此调用由调用者加锁.
	*/
	bool remove(KHttpObject *obj) {
		//std::map<KUrl *, KHttpObjectNode *,lessurl>::iterator it;
		//		printf("KHttpObject:try to remove obj from hash url=%s,this=%x\n",obj->url->host,this);
		//lock.Lock(__FILE__,__LINE__);
		krb_node *node = find(&obj->uk, nodes.rb_node, NULL);
		kassert(node != NULL);
		if (node == NULL) {
			klog(KLOG_ERR, "BUG!!!cache system cann't find obj [%s%s%s%s] to remov\n",
				obj->uk.url->host,
				obj->uk.url->path,
				(obj->uk.url->param ? "?" : ""),
				(obj->uk.url->param ? obj->uk.url->param : "")
			);
			return false;
		}
		KHttpObject *objnode = (KHttpObject *)(node->data);
		kassert(objnode);
		KHttpObject *last = NULL;
		bool result = false;
		while (objnode) {
			if (obj == objnode) {
				result = true;
				if (last == NULL) {
					node->data = objnode->next;
				} else {
					last->next = objnode->next;
				}
				break;
			}
			last = objnode;
			objnode = objnode->next;
		}
		kassert(result);
		if (node->data == NULL) {
			rb_erase(node, &nodes);
			delete node;
		}
		if (result) {
			if (KBIT_TEST(obj->index.flags, FLAG_IN_MEM)) {
				DecMemObjectSize(obj);
			}
			if (KBIT_TEST(obj->index.flags, FLAG_IN_DISK)) {
				DecDiskObjectSize(obj);
			}
		}
		return result;
	}
	/**
	* 清除物件
	*/
	inline int purgeObject(KHttpObject *objnode,objHandler handle,void *param)
	{
		int count = 0;
		while (objnode) {
			if (!KBIT_TEST(objnode->index.flags,FLAG_DEAD)) {
				handle(objnode,param);
				count++;
			}
			objnode = objnode->next;
		}
		return count;
	}
	inline void insert(KHttpObject *obj)
	{
		struct krb_node **n = &(nodes.rb_node), *parent = NULL;
		KHttpObject *objnode = NULL;
		struct krb_node *match_url_obj = NULL;
		/* Figure out where to put new node */		
		while (*n) {
			bool match_url = false;
			objnode = (KHttpObject *)((*n)->data);
			int result = cmp_url_key(&obj->uk,&objnode->uk,match_url);
			if (match_url && match_url_obj==NULL) {
				match_url_obj = *n;
				//printf("insert match_url_obj=[%p]\n", match_url_obj);
			}
			parent = *n;
			if (result < 0) {
				n = &((*n)->rb_left);
			} else if (result > 0) {
				n = &((*n)->rb_right);
			} else {
				kassert((*n)->data);
				kassert(match_url_obj);
				if (cmp_vary_key(((KHttpObject *)(match_url_obj->data))->uk.vary, obj->uk.vary) != 0) {
					//vary is changed
					InternalUpdateVary(match_url_obj, obj->uk.vary);
				}
				obj->next = objnode;
				(*n)->data = obj;
				return;
			}
		}
		if (match_url_obj && cmp_vary_key(((KHttpObject *)(match_url_obj->data))->uk.vary, obj->uk.vary) != 0) {
			//vary is changed
			InternalUpdateVary(match_url_obj, obj->uk.vary);
		}
		krb_node *node = new krb_node;
		memset(node, 0, sizeof(krb_node));
		node->data = obj;
		obj->next = NULL;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &nodes);
	}
	inline krb_node *findn(KUrlKey *uk,int path_len)
	{
		struct krb_node *last = NULL;
		struct krb_node *node = nodes.rb_node;
		while (node) {
			KHttpObject *data = (KHttpObject *)(node->data);
			int result;
			result = cmpn_url_key(uk,&data->uk,path_len);
			if (result < 0) {
				node = node->rb_left;
			} else if (result > 0) {
				node = node->rb_right;
			} else {
				for(;;) {
					last = rb_prev(node);
					if (last==NULL) {
						return node;
					}
					data = (KHttpObject *)(last->data);
					if (cmpn_url_key(uk, &data->uk ,path_len)!=0) {
						return node;
					}
					node = last;
				}
			}
		}
		return NULL;
	}
	/*
	查找uk,从node开始，match_url_node用于返回首次命中url(不带vary)的节点。
	*/
	inline krb_node *find(KUrlKey *uk, krb_node *node,krb_node **match_url_node)
	{
		bool match_url = false;
		while (node) {
			KHttpObject *data = (KHttpObject *)(node->data);
			int result;
			result = cmp_url_key(uk,&data->uk,match_url);
			if (match_url && match_url_node && *match_url_node==NULL) {
				*match_url_node = node;
			}
			if (result < 0) {
				node = node->rb_left;
			} else if (result > 0) {
				node = node->rb_right;
			} else {
				return node;
			}
		}
		return NULL;
	}
	struct krb_root nodes ;
};
#endif /*KHTTPOBJECTHASH_H_*/
