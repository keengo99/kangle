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
#if 0
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
#endif
class KDiskCacheStream {
public:
	KDiskCacheStream() : filename(nullptr), fp(nullptr), buffer(nullptr),
		hot(nullptr), buffer_left(0), buffer_size(0) {}
	~KDiskCacheStream()
	{
		reset(fp != nullptr);
		if (buffer) {
			aio_free_buffer(buffer);
		}
	}
	int64_t GetLength(KHttpObject* obj);
	bool Open(KHttpObject *obj);
	bool Write(KHttpObject *obj,const char *buf, int len);
	bool Close(KHttpObject *obj);
private:
	void reset(bool remove_file) {
		if (fp) {
			kfiber_file_close(fp);
			fp = nullptr;
		}
		if (filename) {
			if (remove_file) {
				unlink(filename);
			}
			xfree(filename);
			filename = nullptr;
		}
		hot = buffer;
		buffer_left = buffer ? buffer_size : 0;
	}
	bool FlushBuffer(bool last = false);
	char *filename;
	kfiber_file* fp;
	char* buffer;
	char* hot;
	int buffer_left;
	int buffer_size;
};
#endif
#endif
