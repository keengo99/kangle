#include <vector>
#include "lib.h"
#include "KHttpObjectHash.h"
#include "KObjectList.h"
#include "cache.h"
#include "KReg.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "kmalloc.h"
#include "KCache.h"
#define CLEAN_CACHE		0
#define DROP_DEAD		1
#define MAX_LOCK_MOVE_SIZE 1
#define MAX_CLEAN_DEAD_COUNT 1024

KObjectList::KObjectList()
{
	l_head = l_end = NULL;
	this->list_state = 0;
	cache_model = CLEAN_CACHE;
}
void KObjectList::add(KHttpObject *m_list)
{	 
	assert(m_list->list_state == LIST_IN_NONE);
	m_list->list_state = list_state;
	if (l_end == NULL) {
		l_end = m_list;
		l_head = m_list;
		m_list->lnext = NULL;
		m_list->lprev = NULL;
	} else {
		l_end->lnext = m_list;
		m_list->lprev = l_end;
		l_end = m_list;
		l_end->lnext = NULL;
	}
#ifndef NDEBUG
	checkList();
#endif
}
#ifdef ENABLE_DISK_CACHE
#if 0
int KObjectList::save_disk_index(KFile *fp)
{
	int save_count = 0;
	KHttpObject *obj = l_end;
	while(obj){
		if(KBIT_TEST(obj->index.flags,FLAG_IN_DISK)){
			if(!obj->saveIndex(fp)){
				return -1;
			}
			save_count++;
		}
		obj = obj->lprev;
	}
	return save_count;
}
#endif
void KObjectList::swap_all_obj()
{
	KBufferFile bf;
	cache.lock();
	KHttpObject *obj = l_head;
	while (obj) {
		if (!KBIT_TEST(obj->index.flags, FLAG_DEAD)) {
			obj->swapout(&bf,true);
		}
		obj = obj->lnext;
	}
	cache.unlock();
}
#endif
void KObjectList::remove(KHttpObject *m_list)
{
	assert(m_list->list_state == this->list_state);
	if (m_list == l_head){
		l_head = l_head->lnext;
	}
	if (m_list == l_end) {
		l_end = m_list->lprev;
	}
	if (m_list->lnext){
		m_list->lnext->lprev = m_list->lprev;
	}
	if (m_list->lprev){
		m_list->lprev->lnext = m_list->lnext;
	}
#ifndef NDEBUG	
	m_list->list_state = LIST_IN_NONE;
	checkList();
#endif
}
void KObjectList::dump_refs_obj(std::stringstream &s)
{
	int max_dump = 50;
	KHttpObject *obj = l_head;
	while (obj) {
		if (obj->refs>1) {
			s << obj->refs << " " << obj->uk.url->host << obj->uk.url->path;
			if (obj->uk.url->param) {
				s << "?" << obj->uk.url->param;
			}
			s << "\r\n";
			if (max_dump--<=0) {
				break;
			}
		}
		obj = obj->lnext;
	}
}
void KObjectList::dead(KHttpObject *m_list)
{
	assert(m_list->list_state == this->list_state);
	cache_model = DROP_DEAD;
	if (m_list == l_head) {
		return;
	}
	if (m_list == l_end) {
		l_end = m_list->lprev;
	}
	if (m_list->lnext) {
		m_list->lnext->lprev = m_list->lprev;
	}
	if (m_list->lprev) {
		m_list->lprev->lnext = m_list->lnext;
	}
	l_head->lprev = m_list;
	m_list->lnext = l_head;
	l_head = m_list;
	l_head->lprev = NULL;
#ifndef NDEBUG
	checkList();
#endif
}
#ifdef MALLOCDEBUG
void KObjectList::free_all_cache()
{
	KHttpObject *obj = l_head;
	while (obj) {
		assert(obj->refs==1);
		KHttpObject *next = obj->lnext;
		KBIT_CLR(obj->index.flags,FLAG_IN_DISK);
		cache.objHash[obj->h].remove(obj);
		obj->release();
		obj = next;
	}
}
#endif
void KObjectList::dead_all_obj()
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	while (obj) {
		obj->Dead();
		obj = obj->lnext;
	}
}
void KObjectList::dead_count(int &count)
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	while (obj && count>0) {
		obj->Dead();
		obj = obj->lnext;
		count--;
	}
}
int KObjectList::move(KBufferFile *bf, int64_t begin_msec,INT64 m_size,bool swapout_flag)
{
	bool is_dead = true;
	int dead_obj_count = 0;
	cache.lock();
	if (m_size<=0 && cache_model==CLEAN_CACHE) {
		cache.unlock();
		return 0;
	}
	KTempHttpObject *thead = NULL;
	KHttpObject * obj = l_head;
	while (obj) {	
		if (obj->getRefs() > 1) {
			obj = obj->lnext;
			continue;
		}
		is_dead = (KBIT_TEST(obj->index.flags,FLAG_DEAD)>0);
		if (m_size <= 0 && is_dead) {
			if (dead_obj_count++ > MAX_CLEAN_DEAD_COUNT) {
				//一次性最大清理死亡物件，防止点清理所有缓存时，而卡住非常长的时间
				break;
			}
		}
		if (!is_dead && m_size <= 0) {
			cache_model = CLEAN_CACHE;
			break;
		}
		KTempHttpObject *to = new KTempHttpObject;
		to->decSize = obj->index.head_size;
		if (!swapout_flag || (obj->data && obj->data->type == MEMORY_OBJECT)) {
			//磁盘缓存清理或者是内存交换大物件
			to->decSize += obj->index.content_length;
		}
		if (swapout_flag && KBIT_TEST(obj->index.flags, FLAG_NO_DISK_CACHE)) {
			//标记为不使用磁盘缓存的，直接dead
			is_dead = true;
		}
		m_size -= to->decSize;
		//加入到临时链表中			
		to->next = thead;
		thead = to;
		assert(obj->list_state == this->list_state);
		to->obj = obj;
		to->is_dead = is_dead;		
		obj = obj->lnext;
	}
	cache.unlock();
	//实际发生磁盘IO
	int count = 0;
	while (thead) {
		count++;
		KTempHttpObject *tnext = thead->next;
		int gc_used_msec = (int)(kgl_current_msec - begin_msec);
		if (swapout_flag && !cache.is_disk_shutdown() && !thead->is_dead) {
			swapout(thead, bf,gc_used_msec);
		} else {
			swapout_result(thead, gc_used_msec, false);
		}
		thead = tnext;
	}
	return count;
}
void KObjectList::swapout(KTempHttpObject *thead, KBufferFile *bf,int gc_used_msec)
{
#ifdef ENABLE_DISK_CACHE
	if (bf == NULL) {
		bf = new KBufferFile;
	}
	bool result = thead->obj->swapout(bf,gc_used_msec > GC_SLEEP_MSEC);
#else
	bool result = false;
#endif
	swapout_result(thead, gc_used_msec,result);
}
void KObjectList::swapout_result(KTempHttpObject *thead,int gc_used_msec,bool result)
{
	KMutex * lock = NULL;
	KHttpObject *obj = thead->obj;
	cache.lock();
	cache.clean_blocked = (gc_used_msec > (GC_SLEEP_MSEC + 5000));
#ifdef ENABLE_DISK_CACHE
	if (result) {
		lock = obj->getLock();
		lock->Lock();
		KBIT_SET(obj->index.flags, FLAG_IN_DISK);
		if (obj->list_state == this->list_state && obj->refs <= 1) {
			remove(obj);
			KBIT_CLR(obj->index.flags, FLAG_IN_MEM);
			cache.objHash[obj->h].DecMemObjectSize(obj);
			cache.objList[LIST_IN_DISK].add(obj);
			delete obj->data;
			obj->data = NULL;
		}
		lock->Unlock();
		cache.unlock();
		delete thead;
		return;
	}
#endif
	bool removed_result = false;
	lock = &cache.objHash[obj->h].lock;
	lock->Lock();
	//这里为什么要判断一下list_state呢？
	//因为有可能在中间，obj被其它请求使用，如调用swap_in而改变了list_state.
	if (obj->list_state == this->list_state && obj->getRefs() <= 1) {
		remove(obj);
		removed_result = cache.objHash[obj->h].remove(obj);
		cache.count--;
	}
	lock->Unlock();
	cache.unlock();
	if (removed_result) {
		obj->release();
	}
	delete thead;
}
int KObjectList::clean_cache(KReg *reg,int flag)
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	int result = 0;
	while (obj) {
		if (!KBIT_TEST(obj->index.flags,FLAG_DEAD)) {
			KStringBuf url;
			if (obj->uk.url->GetUrl(url) && reg->match(url.getString(),url.getSize(),flag)>0) {
				result++;
				obj->Dead();
			}			
		}
		obj = obj->lnext;
	}
	return result;
}
void KObjectList::getSize(INT64 &csize,INT64 &cdsiz)
{
	KHttpObject *obj = l_head;
	while (obj) {
		if (KBIT_TEST(obj->index.flags,FLAG_IN_MEM)) {
			csize += obj->index.content_length + obj->index.head_size;			
		}
		if (KBIT_TEST(obj->index.flags,FLAG_IN_DISK)) {
			cdsiz += obj->index.content_length + obj->index.head_size;
		}		
		obj = obj->lnext;
	}
}
