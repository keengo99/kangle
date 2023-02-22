#ifndef KFILESTREAM_H
#define KFILESTREAM_H
#include "KHttpStream.h"
#include "KFileName.h"
#include "kfiber.h"
class KFileStream : public KWStream {
public:
	bool open(const char *str,fileModel model=fileWrite)
	{
		return fp.open(str, model);
	}
	int write(const char *buf, int len) override
	{
		return fp.write(buf, len);
	}
	KGL_RESULT write_end(KGL_RESULT result) override {
		if (last_modified > 0) {
			if (!kfutime(fp.getHandle(), last_modified)) {
				//printf("update modifi time=[%x] error\n", (unsigned)last_modified);
			}
		}
		fp.close();
		return STREAM_WRITE_SUCCESS;
	}
	time_t last_modified;
private:
	KFile fp;
};
class KAsyncFileStream : public KWStream
{
public:
	~KAsyncFileStream() {
		if (fp) {
			kfiber_file_close(fp);
		}
	}
	KAsyncFileStream(kasync_file* fp) {
		this->fp = fp;
	}
	int write(const char* buf, int len) override {
		return kfiber_file_write(fp, buf, len);
	}
	KGL_RESULT write_end(KGL_RESULT result) override {
		kfiber_file_close(fp);
		fp = nullptr;
		return result;
	}
private:
	kasync_file* fp;
};

#endif
