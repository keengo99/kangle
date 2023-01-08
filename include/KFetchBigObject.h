#ifndef KFETCHBIGOBJECT_H
#define KFETCHBIGOBJECT_H
#include "KFetchObject.h"
#include "kselector.h"
#include "kasync_file.h"
#include "http.h"
#include "kfiber.h"
#define MIN_SENDFILE_SIZE 4096

#ifdef ENABLE_BIG_OBJECT
class KFetchBigObject : public KFetchObject
{
public:
	KFetchBigObject()
	{
	}
	~KFetchBigObject()
	{

	}
	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override;
};
#endif

#endif
