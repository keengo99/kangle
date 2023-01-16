#ifndef KDIRECTORYFETCHOBJECT_H
#define KDIRECTORYFETCHOBJECT_H
#include "KFetchObject.h"
#include "kforwin32.h"
#include "KBuffer.h"
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#endif
class KPrevDirectoryFetchObject : public KFetchObject
{
public:
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
};
class KDirectoryFetchObject : public KFetchObject
{
public:
	KDirectoryFetchObject();
	~KDirectoryFetchObject();
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
private:
	KGL_RESULT Write(KHttpRequest *rq, kgl_response_body* st,const char *path);
#ifdef _WIN32
	HANDLE dp;
	WIN32_FIND_DATA FileData;
#else
	DIR *dp;
#endif
};
#endif
