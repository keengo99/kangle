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

inline void set_buffer_obj(KAutoBuffer *buffer,KHttpObject *obj)
{
	assert(obj->data->bodys==NULL);
	obj->index.content_length = buffer->getLen();
	obj->data->bodys = buffer->stealBuff();
	KBIT_SET(obj->index.flags,OBJ_IS_READY);
}
class KCacheStream : public KHttpStream
{
public:
	KCacheStream(KWriteStream *st, bool autoDelete) : KHttpStream(st, autoDelete)
	{

	}
	~KCacheStream();
	void init(KHttpRequest *rq, KHttpObject *obj, cache_model cache_layer);
	StreamState write_direct(void *rq, char *buf,int len) override;
	StreamState write_all(void*rq, const char *buf,int len)override;
	StreamState write_end(void*rq, KGL_RESULT result)override;
private:
	void CheckMemoryCacheSize(KHttpRequest *rq);
#ifdef ENABLE_DISK_CACHE	
	KDiskCacheStream *NewDiskCache(KHttpRequest *rq);
	KDiskCacheStream *disk_cache = nullptr;
#endif
	KHttpObject *obj = nullptr;
	KAutoBuffer *buffer = nullptr;
};
#endif
