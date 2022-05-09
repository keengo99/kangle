#ifndef KFILESTREAM_H
#define KFILESTREAM_H
#include "KHttpStream.h"
#include "KFileName.h"

class KFileStream : public KWriteStream {
public:
	bool open(const char *str,fileModel model=fileWrite)
	{
		return fp.open(str, model);
	}
	int write(void *rq, const char *buf, int len) override
	{
		return fp.write(buf, len);
	}
	KGL_RESULT write_end(void*rq, KGL_RESULT result) override {
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
#endif
