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
	int write(KHttpRequest *rq, const char *buf, int len)
	{
		return fp.write(buf, len);
	}
	StreamState write_end(KHttpRequest *rq) {
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
