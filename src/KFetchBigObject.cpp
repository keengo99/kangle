#include "KFetchBigObject.h"

#ifdef ENABLE_BIG_OBJECT
static kfiber_file* open_big_file(KHttpRequest* rq, KHttpObject* obj, INT64 start) {
	if (obj->dk.filename1 == 0) {
		char* url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "BUG bigobj filename1 is 0 url=[%s]\n", url);
		free(url);
		return NULL;
	}
	char* filename = obj->getFileName();
	if (filename == NULL) {
		return NULL;
	}
	kfiber_file* fp = kfiber_file_open(filename, fileRead, 0);
	if (fp == NULL) {
		obj->Dead();
		char* url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "cann't open bigobj cache file [%s] url=[%s]\n", filename, url);
		xfree(filename);
		xfree(url);
		return NULL;
	}
	xfree(filename);
	kfiber_file_seek(fp, seekBegin, (INT64)obj->index.head_size + start);
	return fp;
}
KGL_RESULT KFetchBigObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	assert(in == NULL);
	assert(out == NULL);
	INT64 start;
	build_memory_obj_header(rq, rq->ctx->obj, start, rq->ctx->left_read);
	if (KBIT_TEST(rq->ctx->obj->index.flags, FLAG_NO_BODY) || rq->sink->data.meth == METH_HEAD) {
		rq->ctx->left_read = 0;
		return KGL_NO_BODY;
	}
	kfiber_file* file = open_big_file(rq, rq->ctx->obj, start);
	if (file == NULL) {
		return KGL_EIO;
	}
	int buf_size = conf.io_buffer;
	KGL_RESULT result = KGL_OK;
	char* buffer;
	//*
	if (out) {
		if (out->f->support_sendfile(out, rq)) {
			result = out->f->sendfile(out, rq, file, &rq->ctx->left_read);
			kfiber_file_close(file);
			return result;
		}
	} else {
		if (rq->sink->support_sendfile()) {
			result = rq->sendfile(file, &rq->ctx->left_read);
			goto done;
		}
	}
	if (!kasync_file_direct(file,true)) {
		klog(KLOG_ERR,"cann't turn on file direct_on\n");
		result = KGL_EIO;
		goto done;
	}
	buffer = (char*)aio_alloc_buffer(buf_size);
	while (rq->ctx->left_read > 0) {
		int got = kfiber_file_read(file, buffer, (int)(KGL_MIN(rq->ctx->left_read, (INT64)buf_size)));
		if (got <= 0) {
			result = KGL_EIO;
			break;
		}
		assert(got<=rq->ctx->left_read && got <= buf_size);
		rq->ctx->left_read -= got;
		if (!rq->write_all(kfiber_file_adjust(file, buffer), got)) {
			result = KGL_ESOCKET_BROKEN;
			break;
		}
	}
	aio_free_buffer(buffer);
done:
	kfiber_file_close(file);
	return result;
}
#endif

