#include<vector>
#include "KCacheStream.h"
#include "do_config.h"
#include "KCache.h"
KCacheStream::~KCacheStream()
{
	if (buffer) {
		delete buffer;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		delete disk_cache;
	}
#endif
}
void KCacheStream::init(KHttpRequest *rq, KHttpObject *obj, cache_model cache_layer)
{
	this->obj = obj;
#ifdef ENABLE_DISK_CACHE
	if (cache_layer != cache_memory) {
		buffer = NULL;
		disk_cache = NewDiskCache(rq);
		return;
	}
	disk_cache = NULL;
#endif
	buffer = new KAutoBuffer();
}
StreamState KCacheStream::write_end(void *arg, KGL_RESULT result)
{
	auto rq = (KHttpRequest*)arg;
	if (result != KGL_OK) {
		return KHttpStream::write_end(rq, result);
	}
	bool have_cache = (buffer != NULL);
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		have_cache = true;
	}
#endif
	if (have_cache && obj->data->i.status_code == STATUS_CONTENT_PARTIAL) {
		kassert(obj->IsContentRangeComplete(rq));
		obj->data->i.status_code = STATUS_OK;
		obj->removeHttpHeader(_KS("Content-Range"));
		KBIT_CLR(obj->index.flags, ANSW_HAS_CONTENT_RANGE);
	}
	if (buffer) {
		set_buffer_obj(buffer,obj);
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		kassert(buffer == NULL);		
		if (disk_cache->Close(obj)) {
			kassert(obj->data->bodys == NULL);
			KBIT_SET(obj->index.flags, OBJ_IS_READY | FLAG_IN_DISK);
			obj->data->i.type = BIG_OBJECT;
		}
	}
#endif
	return KHttpStream::write_end(rq, result);
}
void KCacheStream::CheckMemoryCacheSize(KHttpRequest *rq)
{
	if (buffer->getLen() <= conf.max_cache_size) {
		return;
	}
#ifdef ENABLE_DISK_CACHE
	if (obj_can_disk_cache(rq, obj)) {
		//turn on disk cache
		kassert(disk_cache == NULL);
		disk_cache = NewDiskCache(rq);
		if (disk_cache) {
			kbuf *buf = buffer->getHead();
			while (buf) {
				if (buf->used > 0 && !disk_cache->Write(rq, obj, buf->data, buf->used)) {
					delete disk_cache;
					disk_cache = NULL;
					break;
				}
				buf = buf->next;
			}
		}
	}
#endif
	delete buffer;
	buffer = NULL;
}
StreamState KCacheStream::write_direct(void *arg, char *buf,int len)
{
	auto rq = (KHttpRequest*)arg;
	StreamState result = KHttpStream::write_all(rq, buf,len);
	if (buffer) {
		buffer->write_direct(buf, len);
#ifdef ENABLE_DISK_CACHE
		kassert(disk_cache == NULL);
#endif
		CheckMemoryCacheSize(rq);
		return result;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache && !disk_cache->Write(rq,obj, buf, len)) {
		delete disk_cache;
		disk_cache = NULL;		
	}
#endif
	xfree(buf);
	return result;
}

StreamState KCacheStream::write_all(void *arg, const char *buf,int len)
{
	auto rq = (KHttpRequest*)arg;
	StreamState result = KHttpStream::write_all(rq, buf,len);
	if (buffer) {
		buffer->write_all(buf, len);
#ifdef ENABLE_DISK_CACHE
		kassert(disk_cache == NULL);
#endif
		CheckMemoryCacheSize(rq);
		return result;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache && !disk_cache->Write(rq, obj, buf, len)) {
		delete disk_cache;
		disk_cache = NULL;		
	}
#endif
	return result;
}
#ifdef ENABLE_DISK_CACHE
KDiskCacheStream *KCacheStream::NewDiskCache(KHttpRequest *rq)
{
	KDiskCacheStream *disk_cache = new KDiskCacheStream;
	if (!disk_cache->Open(rq, obj)) {
		delete disk_cache;
		return NULL;
	}
	return disk_cache;
}
#endif
