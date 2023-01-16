#ifndef KCACHESTREAM_H
#define KCACHESTREAM_H
#include "KHttpStream.h"
#include "KHttpObject.h"
#include "KDiskCacheStream.h"

enum cache_model
{
	cache_none,
	cache_memory,
	cache_disk
};


struct KCacheStream : kgl_forward_body
{
public:
	~KCacheStream() {
		if (buffer) {
			delete buffer;
		}
#ifdef ENABLE_DISK_CACHE
		if (disk_cache) {
			delete disk_cache;
		}
#endif
	}
	void init(KHttpObject *obj, cache_model cache_layer);
	StreamState write_all(const char *buf,int len);
	StreamState write_end(KGL_RESULT result);
	void CheckMemoryCacheSize();
#ifdef ENABLE_DISK_CACHE	
	KDiskCacheStream *NewDiskCache();
	KDiskCacheStream *disk_cache = nullptr;
#endif
	KHttpObject *obj = nullptr;
	KAutoBuffer *buffer = nullptr;
};
bool pipe_cache_stream(KHttpRequest* rq, KHttpObject* obj, cache_model cache_layer, kgl_response_body* body);
#endif
