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
#include "KStringBuf.h"

#ifdef ENABLE_TF_EXCHANGE
#define TEMPFILE_POST_CHUNK_SIZE     8192
/*
 * The first TEMPFILE_MEMORY_SIZE bytes stay in memory; only data past that
 * goes to a file. Most responses never create the file.
 */
#define TEMPFILE_MEMORY_SIZE         65536
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
	bool open_file();
	kfiber* wait_read;
	volatile int64_t write_offset;
	volatile int64_t read_offset;
	kfiber_file* fp;
	kfiber_mutex* lock;
	KString file;
	char* mem;
	int mem_size;
	volatile bool write_is_end;
	volatile bool closed;
};

/*
 * Read the whole request body into a temp file. max_size > 0 limits the
 * body size; when it is exceeded, false is returned and *too_large is set.
 */
bool new_tempfile_input_stream(kgl_input_stream* in, int64_t max_size = 0, bool* too_large = nullptr);
bool tee_tempfile_body(kgl_response_body * body);
//读post数据到临时文件
KTHREAD_FUNCTION clean_tempfile_thread(void *param);
#endif
#endif
