#ifndef KSTATICFETCHOBJECT_H
#define KSTATICFETCHOBJECT_H
#include <stdlib.h>
#include "global.h"
#include "KFetchObject.h"
#include "kasync_file.h"
#include "kfiber.h"

class KStaticFetchObject : public KFetchObject 
{
public:	
	KStaticFetchObject():KFetchObject(0)
	{
		fp = NULL;
	}
	~KStaticFetchObject()
	{
		assert(fp == NULL);
		Close(NULL);
	}
	void Close(KHttpRequest* rq)
	{
		if (fp) {
			kfiber_file_close(fp);
			fp = NULL;
		}
	}
	KGL_RESULT Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out) override;
private:
	KGL_RESULT InternalProcess(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
	kfiber_file* fp;
};
#endif
