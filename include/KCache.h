#ifndef KCACHE_H_22
#define KCACHE_H_22
#include "global.h"
#include "KMutex.h"
#include "KHttpObject.h"
#include "KObjectList.h"
#include "kmalloc.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "KHttpObjectHash.h"
extern volatile uint32_t disk_file_base;
void handle_purge_object(KHttpObject *obj,void *param);
void handle_cache_info(KHttpObject *obj,void *param);
struct KCacheInfo
{
	INT64 mem_size;
	INT64 disk_size;
	int mem_count;
	int disk_count;
};

class KCache
{
public:
	KCache();
	void init(bool firstTime = false);
	void rate(KHttpObject *obj)
	{
		if (obj->in_cache == 0) {
			return;
		}
		lock();
		assert(obj->list_state != LIST_IN_NONE);
		objList[obj->list_state].remove(obj);
		if (KBIT_TEST(obj->index.flags, FLAG_IN_MEM)) {
			objList[LIST_IN_MEM].add(obj);
		}
		else {
			objList[LIST_IN_DISK].add(obj);
		}
		unlock();
#ifdef ENABLE_DB_DISK_INDEX
		/*
		if (KBIT_TEST(obj->index.flags, FLAG_IN_DISK) &&
			dci &&
			dci->getWorker()->getQueue()<1024) {
			dci->start(ci_updateLast, obj);
		}
		*/
#endif
	}
	//obj
	void dead(KHttpObject *obj,const char *file,int line)
	{
		if (obj->in_cache == 0) {
			return;
		}
		klog(KLOG_DEBUG, "dead obj=[%p] from [%s:%d]\n", obj, file, line);
		cacheLock.Lock();
		assert(obj->list_state != LIST_IN_NONE);
		obj->Dead();
		objList[obj->list_state].dead(obj);
		cacheLock.Unlock();
	}
	KHttpObject * find(KHttpRequest *rq, u_short url_hash)
	{
		return objHash[url_hash].get(rq);
	}
	void iterator(objHandler handler, void *param);
	bool add(KHttpObject *obj, int list_state)
	{
		if (obj->h == HASH_SIZE) {
			obj->h = hash_url(obj->uk.url);
		}
		assert(obj->in_cache == 0);		
		cacheLock.Lock();
		if (clean_blocked) {
			cacheLock.Unlock();
			//klog(KLOG_INFO, "clean_blocked add to cache failed\n");
			return false;
		}
		obj->in_cache = 1;
		if (!objHash[obj->h].put(obj)) {
			obj->in_cache = 0;
			cacheLock.Unlock();
			return false;
		}
		objList[list_state].add(obj);
		count++;
		cacheLock.Unlock();
		return true;
	}
#ifdef ENABLE_DISK_CACHE
#if 0
	int save_disk_index(KFile *fp)
	{
		int save_count = 0;
		lock();
		for (int i = 1; i >= 0; i--) {
			int this_save_count = objList[i].save_disk_index(fp);
			if (this_save_count == -1) {
				break;
			}
			save_count += this_save_count;
		}
		unlock();
		return save_count;
	}
#endif
#endif
	void flush_mem_to_disk()
	{
		KBufferFile bf;
		objList[LIST_IN_MEM].move(&bf,kgl_current_msec,1, true);
	}
	bool check_count_limit(INT64 maxMemSize)
	{
		int max_count = int(maxMemSize / 1024);
		cacheLock.Lock();
		clean_blocked= false;
		int cache_count = count;
		if (cache_count <= max_count) {
			cacheLock.Unlock();
			return false;
		}
		int kill_count = cache_count - max_count;
		objList[LIST_IN_DISK].dead_count(kill_count);
		objList[LIST_IN_MEM].dead_count(kill_count);
		cacheLock.Unlock();
		klog(KLOG_ERR, "cache count limit kill count=[%d]\n", kill_count);
		return true;
	}
	void flush(int64_t last_msec,INT64 maxMemSize,INT64 maxDiskSize,bool disk_is_radio)
	{
		INT64 total_size = 0;
		INT64 kill_mem_size=0,kill_disk_size =0;
		INT64 total_disk_size = 0;
		int disk_clean_count = 0;
		katom_inc((void *)&disk_file_base);
		check_count_limit(maxMemSize);
		int mem_count = 0, disk_count = 0;
		getSize(total_size, total_disk_size,mem_count,disk_count);
		kill_mem_size = total_size - maxMemSize;
		int memory_clean_count = objList[LIST_IN_MEM].move(&bf, last_msec,kill_mem_size, true);
#ifdef ENABLE_DISK_CACHE
		if (disk_is_radio) {
			kill_disk_size = get_need_free_disk_size((int)(maxDiskSize));
		} else {
			kill_disk_size = total_disk_size - maxDiskSize;
			INT64 kill_percent_disk_size = get_need_free_disk_size(95);
			if (kill_percent_disk_size > kill_disk_size) {
				//最大不超过磁盘95%
				kill_disk_size = kill_percent_disk_size;
			}
		}
		disk_clean_count = objList[LIST_IN_DISK].move(&bf, last_msec,kill_disk_size, false);
		if (disk_clean_count == 0 && kill_disk_size > 0) {
			//disk list不够，则从mem list删除
			disk_clean_count = objList[LIST_IN_MEM].move(&bf, last_msec, kill_disk_size, false);
			klog(KLOG_INFO, "clean disk list count is zero,now clean it from memory list.\n");
		}
#endif
		klog(KLOG_DEBUG, "cache flush, killed memory [" INT64_FORMAT " %d],killed disk [" INT64_FORMAT " %d]\n",
			kill_mem_size,
			memory_clean_count,
			kill_disk_size,
			disk_clean_count);
		return;
	}
	void getSize(INT64 &memSize,INT64 &diskSize,int &mem_count,int &disk_count)
	{
		int i;
		for (i = 0; i < HASH_SIZE; i++) {
			objHash[i].getSize(memSize, diskSize,mem_count,disk_count);
		}
	}
#ifdef MALLOCDEBUG
	void freeAllObject()
	{
		for (int i=0;i<2;i++) {
			objList[i].free_all_cache();
		}
	}
#endif
	int getCount()
	{
		return count;
	}
	int clean_cache(KReg *reg,int flag)
	{
		int result = 0;
		cacheLock.Lock();
		for (int i=0;i<2;i++) {
			result += objList[i].clean_cache(reg,flag);
		}
		cacheLock.Unlock();
		return result;
	}
	int clean_cache(KUrl *url,bool wide)
	{
		int count = 0;
		for (int i=0;i<HASH_SIZE;i++) {
			count += objHash[i].purge(url,wide,handle_purge_object,NULL);
		}
		return count;
	}
	bool IsCleanBlocked()
	{
		return clean_blocked;
	}
	int get_cache_info(KUrl*url,bool wide,KCacheInfo *ci)
	{
		int count = 0;
		for (int i=0;i<HASH_SIZE;i++) {
			count += objHash[i].purge(url,wide,handle_cache_info,ci);
		}
		return count;
	}
	void dead_all_obj()
	{
		cacheLock.Lock();
		for(int i=0;i<2;i++){
			objList[i].dead_all_obj();
		}
		cacheLock.Unlock();
	}
	void syncDisk()
	{
		cacheLock.Lock();
#ifdef ENABLE_DB_DISK_INDEX
		for (int i=0;i<2;i++) {
			KHttpObject *obj = objList[i].getHead();
			while (obj) {
				if (KBIT_TEST(obj->index.flags,FLAG_IN_DISK) && obj->dc_index_update) {
					obj->dc_index_update = 0;
					if (dci && obj->data) {
						dci->start(ci_update,obj);
					}
				}
				obj = obj->lnext;
			}
		}
#endif
		cacheLock.Unlock();
	}
	bool IsDiskCacheReady()
	{
		if (disk_shutdown) {
			return false;
		}
		if (clean_blocked) {
			return false;
		}
#ifdef ENABLE_DB_DISK_INDEX
		if (dci == NULL) {
			return false;
		}
#endif
		return true;
	}
	bool is_disk_shutdown()
	{
		return disk_shutdown;
	}
	void shutdown_disk(bool shutdown_flag)
	{
		disk_shutdown = shutdown_flag;
	}
	void UpdateVary(KHttpObject *obj, KVary *vary);
	KHttpObjectHash *getHash(u_short h)
	{
		return &objHash[h];
	}
	u_short hash_url(KUrl *url) {
#ifdef MULTI_HASH
        u_short res = 0;
        res = string_hash(url->host,1);
        res = string_hash(url->path, res) & HASH_MASK;
        return res;
#else
        return 0;
#endif
}
	friend class KObjectList;
private:
	void lock()
	{
		cacheLock.Lock();
	}
	void unlock()
	{
		cacheLock.Unlock();
	}
	volatile bool clean_blocked;
	volatile bool disk_shutdown;
	int count;
	int mem_count;
	int disk_count;
	KMutex cacheLock;
	KObjectList objList[2];
	KHttpObjectHash objHash[HASH_SIZE];
	KBufferFile bf;
};
extern KCache cache;
#endif
