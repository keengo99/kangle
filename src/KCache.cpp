#include "KCache.h"
KCache cache;
void init_cache() {
	cache.init();
}
KCache::KCache()
{
	count = 0;
	mem_count = 0;
	disk_count = 0;
	clean_blocked = false;
	disk_shutdown = true;
}
void KCache::init(bool firstTime)
{
	if (firstTime) {
		//for(u_short i=0;i<HASH_SIZE;i++){
			//objHash[i].id = i;
		//}
		for(unsigned char i=0;i<2;i++){
			objList[i].list_state = i;
		}
	}
#ifdef ENABLE_DISK_CACHE
	if (conf.disk_cache>0) {
		init_disk_cache(firstTime);
	}
#endif
}
void KCache::UpdateVary(KHttpObject *obj, KVary *vary)
{
	objHash[obj->h].UpdateVary(obj, vary);
}
void handle_purge_object(KHttpObject *obj,void *param)
{
	//char *url = obj->url->getUrl();
	//klog(KLOG_NOTICE,"purge obj [%s]\n",url);
	//free(url);
	obj->Dead();
}
void handle_cache_info(KHttpObject *obj,void *param)
{
	KCacheInfo *ci = (KCacheInfo *)param;
	obj->CountSize(ci->mem_size,ci->disk_size,ci->mem_count,ci->disk_count);
}
