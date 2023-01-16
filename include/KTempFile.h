#ifndef KTEMPFILE_H
#define KTEMPFILE_H
#include "global.h"
#include <stdio.h>
#include <string>
#include "kforwin32.h"
#include "ksapi.h"
#include "KReadWriteBuffer.h"
#include "KFileName.h"
#include "kselector.h"
#include "KHttpStream.h"
#include "kfiber.h"

#ifdef ENABLE_TF_EXCHANGE
#define TEMPFILE_POST_CHUNK_SIZE     8192
class KHttpRequest;
class KTempFile
{
public:
	KTempFile();
	~KTempFile();
	bool Init();
	void WriteEnd();
	bool write_all(const char* buf, int len);
	int Write(const char *buf, int len);
	int Read(char* buf, int len);
	void Close();
	int64_t GetSize()
	{
		return write_offset;
	}
	int64_t GetLeft()
	{
		return write_offset - read_offset;
	}
private:
	kfiber* wait_read;
	volatile int64_t write_offset;
	volatile int64_t read_offset;
	kfiber_file* fp;
	kfiber_mutex* lock;
	std::string file;
	volatile bool write_is_end;
};

bool new_tempfile_input_stream(KHttpRequest* rq, kgl_input_stream* in);
bool new_tempfile_output_stream(KHttpRequest* rq, kgl_output_stream* out);
//读post数据到临时文件
KTHREAD_FUNCTION clean_tempfile_thread(void *param);
#endif
#endif
