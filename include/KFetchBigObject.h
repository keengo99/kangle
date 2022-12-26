#ifndef KFETCHBIGOBJECT_H
#define KFETCHBIGOBJECT_H
#include "KFetchObject.h"
#include "kselector.h"
#include "kasync_file.h"
#include "http.h"
#include "kfiber.h"
#define MIN_SENDFILE_SIZE 4096
//{{ent
#ifdef ENABLE_BIG_OBJECT
class KFetchBigObject : public KFetchObject
{
public:
	KFetchBigObject()
	{
	}
	kfiber_file *InternalOpen(KHttpRequest *rq, KHttpObject *obj, INT64 start)
	{
		if (obj->dk.filename1 == 0) {
			char *url = obj->uk.url->getUrl();
			klog(KLOG_ERR, "BUG bigobj filename1 is 0 url=[%s]\n", url);
			free(url);
			return NULL;
		}
		char *filename = obj->getFileName();
		if (filename==NULL) {
			return NULL;
		}
		kfiber_file *fp = kfiber_file_open(filename, fileRead, KFILE_ASYNC);
		if (fp == NULL) {
			obj->Dead();
			char *url = obj->uk.url->getUrl();
			klog(KLOG_ERR,"cann't open bigobj cache file [%s] url=[%s]\n", filename,url);
			xfree(filename);
			xfree(url);
			return NULL;
		}
		xfree(filename);
		kfiber_file_seek(fp, seekBegin, (INT64)obj->index.head_size + start);
		return fp;
	}
	~KFetchBigObject()
	{

	}
	KGL_RESULT Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out)
	{
		assert(in == NULL);
		assert(out == NULL);
		INT64 start;
		build_memory_obj_header(rq, rq->ctx->obj, start, rq->ctx->left_read);
		if (rq->ctx->left_read == -1) {
			return KGL_NO_BODY;
		}
		kfiber_file* file = InternalOpen(rq, rq->ctx->obj, start);
		if (file==NULL) {
			return KGL_EIO;
		}
		rq->ctx->know_length = 1;
		KGL_RESULT result = KGL_OK;
		int buf_size = conf.io_buffer;
		char *buffer = (char*)aio_alloc_buffer(buf_size);
		while (rq->ctx->left_read > 0) {
			int got = kfiber_file_read(file, buffer, (int)(KGL_MIN(rq->ctx->left_read, (INT64)buf_size)));
			if (got <= 0) {
				result = KGL_EIO;
				break;
			}
			rq->ctx->left_read -= got;
			if (!rq->WriteAll(kfiber_file_adjust(file,buffer), got)) {
				result = KGL_ESOCKET_BROKEN;
				break;
			}
		}
		aio_free_buffer(buffer);
		kfiber_file_close(file);
		return result;
	}
private:
};
#endif
//}}
#endif
