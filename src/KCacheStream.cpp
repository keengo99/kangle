#include<vector>
#include "KCacheStream.h"
#include "do_config.h"
#include "KCache.h"
inline void set_buffer_obj(KAutoBuffer* buffer, KHttpObject* obj) {
	assert(obj->data->bodys == NULL);
	obj->index.content_length = buffer->getLen();
	obj->data->bodys = buffer->stealBuff();
	obj->cache_is_ready = 1;
}
static bool obj_can_use_disk_cache(KHttpObject* obj) {
	if (KBIT_TEST(obj->index.flags, ANSW_LOCAL_SERVER | FLAG_NO_DISK_CACHE)) {
		return false;
	}
	return cache.IsDiskCacheReady();
}

void KCacheStream::init(KHttpObject *obj, cache_model cache_layer)
{
	this->obj = obj;
	assert(KBIT_TEST(obj->index.flags, FLAG_DEAD | ANSW_NO_CACHE) == 0);
#ifdef ENABLE_DISK_CACHE
	if (cache_layer != cache_memory) {
		buffer = NULL;
		disk_cache = NewDiskCache();
		return;
	}
	disk_cache = NULL;
#endif
	buffer = new KAutoBuffer();
}
KGL_RESULT cache_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	KCacheStream* cache = (KCacheStream*)ctx;
	result = cache->down_body.f->close(cache->down_body.ctx, result);
	if (result == KGL_OK) {
		bool have_cache = (cache->buffer != NULL);
#ifdef ENABLE_DISK_CACHE
		if (cache->disk_cache) {
			have_cache = true;
		}
#endif
		if (have_cache && cache->obj->data->i.status_code == STATUS_CONTENT_PARTIAL) {
			cache->obj->data->i.status_code = STATUS_OK;
			cache->obj->remove_http_header(_KS("Content-Range"));
			KBIT_CLR(cache->obj->index.flags, ANSW_HAS_CONTENT_RANGE);
		}
		if (cache->buffer) {
			set_buffer_obj(cache->buffer, cache->obj);
		}
#ifdef ENABLE_DISK_CACHE
		if (cache->disk_cache) {
			kassert(cache->buffer == NULL);
			if (cache->disk_cache->Close(cache->obj)) {
				kassert(cache->obj->data->bodys == NULL);
				KBIT_SET(cache->obj->index.flags, FLAG_IN_DISK);
				cache->obj->cache_is_ready = 1;
				cache->obj->data->i.type = BIG_OBJECT;
			}
		}
#endif
	}
	delete cache;
	return result;
}
void KCacheStream::CheckMemoryCacheSize()
{
	if (buffer->getLen() <= conf.max_cache_size) {
		return;
	}
#ifdef ENABLE_DISK_CACHE
	if (obj_can_use_disk_cache(obj)) {
		//turn on disk cache
		kassert(disk_cache == NULL);
		disk_cache = NewDiskCache();
		if (disk_cache) {
			kbuf *buf = buffer->getHead();
			while (buf) {
				if (buf->used > 0 && !disk_cache->Write(obj, buf->data, buf->used)) {
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

static KGL_RESULT cache_write(kgl_response_body_ctx* ctx, const char* buf, int len)
{
	KCacheStream* cache = (KCacheStream*)ctx;
	StreamState result = cache->down_body.f->write(cache->down_body.ctx, buf, len);
	if (cache->buffer) {
		cache->buffer->write_all(buf, len);
#ifdef ENABLE_DISK_CACHE
		kassert(cache->disk_cache == NULL);
#endif
		cache->CheckMemoryCacheSize();
		return result;
	}
#ifdef ENABLE_DISK_CACHE
	if (cache->disk_cache && !cache->disk_cache->Write(cache->obj, buf, len)) {
		delete cache->disk_cache;
		cache->disk_cache = NULL;		
	}
#endif
	return result;
}
#ifdef ENABLE_DISK_CACHE
KDiskCacheStream *KCacheStream::NewDiskCache()
{
	KDiskCacheStream *disk_cache = new KDiskCacheStream;
	if (!disk_cache->Open(obj)) {
		delete disk_cache;
		return NULL;
	}
	return disk_cache;
}
#endif
static kgl_response_body_function cache_body_function = {
	unsupport_writev<cache_write>,
	cache_write,
	forward_flush,
	support_sendfile_false,
	unsupport_sendfile,
	cache_close
};
bool pipe_cache_stream(KHttpRequest* rq, KHttpObject* obj, cache_model cache_layer, kgl_response_body* body) 	{
	if (KBIT_TEST(obj->index.flags, ANSW_NO_CACHE|FLAG_DEAD) >0 || conf.default_cache == 0) {
		return false;
	}
	if (!KBIT_TEST(rq->ctx.filter_flags, RF_NO_DISK_CACHE)) {
		cache_layer = cache_memory;
	}
	KCacheStream* stream = new KCacheStream;
	stream->init(obj, cache_layer);
	pipe_response_body(stream, &cache_body_function, body);
	return true;
}
