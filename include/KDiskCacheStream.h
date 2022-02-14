#ifndef KDISKCACHESTREAM_H
#define KDISKCACHESTREAM_H
#include "global.h"
#include "kmalloc.h"
#include "kasync_file.h"
#include "kfiber.h"
#ifdef ENABLE_DISK_CACHE
class KHttpObject;
class KHttpRequest;
class KDiskCacheStream;
class KDiskCacheContext {
public:
	KDiskCacheContext()
	{
		memset(this, 0, sizeof(*this));
	}
	~KDiskCacheContext()
	{
		if (buffer) {
			aio_free_buffer(buffer);
		}
	}
	char *buffer;
	char *hot;
	INT64 offset;
	KDiskCacheStream *disk_cache;
	int size;
};
class KDiskCacheStream {
public:
	KDiskCacheStream() {
		memset(this, 0, sizeof(KDiskCacheStream));
	}
	~KDiskCacheStream()
	{
		if (fp) {
			kfiber_file_close(fp);
			kassert(filename);
			unlink(filename);
		}
		if (filename) {
			xfree(filename);
		}
		if (buffer) {
			aio_free_buffer(buffer);
		}
	}
	int64_t GetLength(KHttpObject* obj);
	bool Open(KHttpRequest *rq,KHttpObject *obj);
	bool Write(KHttpRequest *rq, KHttpObject *obj,const char *buf, int len);
	bool Close(KHttpObject *obj);
private:
	bool FlushBuffer();
	char *filename;
	kfiber_file* fp;
	char* buffer;
	char* hot;
	int buffer_left;
};
#endif
#endif
