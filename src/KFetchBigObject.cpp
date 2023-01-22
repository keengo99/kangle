#include "KFetchBigObject.h"
#include "KFilterContext.h"

#ifdef ENABLE_BIG_OBJECT
static kfiber_file* open_big_file(KHttpRequest* rq, KHttpObject* obj, INT64 start) {
	if (obj->dk.filename1 == 0) {
		char* url = obj->uk.url->getUrl();
		klog(KLOG_ERR, "BUG bigobj filename1 is 0 url=[%s]\n", url);
		free(url);
		return NULL;
	}
	char* filename = obj->get_filename();
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
#if 0
	kgl_request_range *range = in->f->get_range(in->ctx);
	rq->ctx.left_read = rq->ctx.obj->index.content_length;
	if (range) {
		if (!kgl_adjust_range(range, &rq->ctx.left_read)) {
			out->f->write_status(out->ctx, STATUS_RANGE_NOT_SATISFIABLE);
			KStringBuf s;
			s.WSTR("*/");
			s.add(rq->ctx.obj->index.content_length, INT64_FORMAT);
			out->f->write_header(out->ctx, kgl_header_content_range, s.getBuf(), s.getSize());
			
		}
	}
#endif
	kgl_response_body body;
	int64_t content_length = rq->ctx.obj->index.content_length;
	get_default_response_body(rq, &body);
	if (rq->needFilter()) {
		int32_t flag = KGL_FILTER_CACHE;
		if (rq->sink->data.range != nullptr) {
			flag |= KGL_FILTER_NOT_CHANGE_LENGTH;
		}
		if (rq->of_ctx->tee_body(rq, &body, flag)) {
			assert(rq->sink->data.range == nullptr);
			content_length = -1;
		}
	}
	rq->ctx.body = body;
	build_obj_header(rq, rq->ctx.obj, content_length, start, rq->ctx.left_read);
	if (rq->sink->data.meth == METH_HEAD) {
		rq->ctx.left_read = 0;
		return body.f->close(body.ctx, KGL_NO_BODY);
	}
	kfiber_file* file = open_big_file(rq, rq->ctx.obj, start);
	if (file == NULL) {
		return KGL_EIO;
	}
	int buf_size = conf.io_buffer;
	KGL_RESULT result = KGL_OK;
	char* buffer;
	//*
	if (body.f->support_sendfile(body.ctx)) {
		result = body.f->sendfile(body.ctx, file, &rq->ctx.left_read);
		goto done;
	}
	
	if (!kasync_file_direct(file,true)) {
		klog(KLOG_ERR,"cann't turn on file direct_on\n");
		result = KGL_EIO;
		goto done;
	}
	buffer = (char*)aio_alloc_buffer(buf_size);
	while (rq->ctx.left_read > 0) {
		int got = kfiber_file_read(file, buffer, (int)(KGL_MIN(rq->ctx.left_read, (INT64)buf_size)));
		if (got <= 0) {
			result = KGL_EIO;
			break;
		}
		assert(got<=rq->ctx.left_read && got <= buf_size);
		rq->ctx.left_read -= got;
		result = body.f->write(body.ctx, kfiber_file_adjust(file, buffer), got);
		if (result!=KGL_OK) {
			break;
		}
	}
	aio_free_buffer(buffer);
done:
	kfiber_file_close(file);
	return body.f->close(body.ctx, result);;
}
#endif

