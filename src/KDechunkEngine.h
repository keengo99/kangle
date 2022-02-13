#ifndef KDECHUNKENGINE_H
#define KDECHUNKENGINE_H
#include "global.h"
#include <stdlib.h>
#include <string.h>
#include "kmalloc.h"

enum dechunk_status
{
	dechunk_success, //解码成功，有数据返回，但要继续解码
	dechunk_continue,//解码成功，有数据返回，要继续喂数据
	dechunk_end,     //解码成功，无数据返回，解码结束
	dechunk_failed   //解码错误
};
class KDechunkEngine {
public:
	KDechunkEngine() {
		chunk_size = 0;
		work_len = 0;
		work = NULL;
	}
	~KDechunkEngine() {
		if (work) {
			free(work);
		}
	}
	//返回dechunk_continue表示还需要读数据，
	dechunk_status dechunk(const char **buf, int &buf_len, const char **piece, int &piece_length);
	bool is_failed() {
		return work_len < -4;
	}
	bool is_success() {
		return work_len == -4;
	}
	bool is_end() {
		return work_len <= -4;
	}
private:
	int chunk_size ;
	int work_len ;
	char *work;
};
#endif
