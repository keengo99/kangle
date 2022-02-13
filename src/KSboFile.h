#ifndef KSBOFILE_H
#define KSBOFILE_H
#include "global.h"
#include "kasync_file.h"
#include "kmalloc.h"
#include "ksapi.h"
#define IO_BLOCK_SIZE 16384
class KHttpRequest;
class KHttpObject;
class KSharedBigObject;
#if 0
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
class KSboFile
{
public:
	KSboFile();
	~KSboFile();
	bool open(KHttpObject *obj,bool create_flag);
	kev_result read(KHttpRequest *rq, char *buffer,INT64 offset, int *length, aio_callback cb, INT64 &next_from);
	bool OpenWrite(INT64 offset,INT64 length);
	KGL_RESULT write(const char *buf, int length);
	bool start_flush();
	//kev_result flush(KHttpRequest *rq, KHttpObject *obj,aio_callback cb);
	void flush_result(int got);
	bool raw_write(KHttpRequest *rq,char *buffer, INT64 offset, int length, aio_callback cb);
	INT64 GetWriteOffset()
	{
		return write_offset;
	}
private:
	char *filename;
	kasync_file *aio_file;
	KSharedBigObject *sbo;
	INT64 write_offset;
	char *write_buffer;
	char *write_hot;
	int write_left;
	int write_buffer_size;
};
#endif//}}
#endif
#endif
