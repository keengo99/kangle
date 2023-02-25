#include "KStaticFetchObject.h"
#include "http.h"
#include "KContentType.h"
#include "KVirtualHostManage.h"
#include "kfiber.h"
#include "KDefer.h"
#include "HttpFiber.h"


KGL_RESULT KStaticFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	KGL_RESULT result = InternalProcess(rq,in, out);
	Close(rq);
	return result;
}
KGL_RESULT KStaticFetchObject::InternalProcess(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	kassert(!rq->file->isDirectory());
	KHttpObject* obj = rq->ctx.obj;
	KBIT_SET(obj->index.flags, ANSW_LOCAL_SERVER);
	kgl_precondition_flag flag;
	kgl_precondition* condition = in->f->get_precondition(in->ctx, &flag);
	time_t last_modified = rq->file->getLastModified();
	if (!kgl_is_safe_method(rq->sink->data.meth)) {
		if (condition) {
			out->f->write_status(out->ctx, STATUS_PRECONDITION);
			return out->f->write_header_finish(out->ctx,0, nullptr);
		}
		out->f->write_status(out->ctx, STATUS_METH_NOT_ALLOWED);
		out->f->write_unknow_header(out->ctx, _KS("Allow"), _KS("GET, HEAD"));
		return out->f->write_header_finish(out->ctx,0, nullptr);
	}
	if (condition && condition->time>0) {
		if (KBIT_TEST(flag, kgl_precondition_mask) != kgl_precondition_if_modified_since) {
			out->f->write_status(out->ctx, STATUS_PRECONDITION);
			return out->f->write_header_finish(out->ctx,0, nullptr);
		}
		if (condition->time >= last_modified) {
			out->f->write_status(out->ctx, STATUS_NOT_MODIFIED);
			return out->f->write_header_finish(out->ctx,0, nullptr);
		}
	}	
	assert(fp == NULL);
	assert(rq->file);
	int file_flag = KFILE_ASYNC;
	kgl_request_range* range = in->f->get_range(in->ctx);
	if (range && !kgl_match_if_range(flag,range, last_modified)) {	
		range = nullptr;
	}
	if (!range) {
		KBIT_SET(file_flag, KFILE_SEQUENTIAL);
	}
	FILE_HANDLE fd = rq->file->open(fileRead, file_flag);
	if (!kflike(fd)) {
		return out->f->error(out->ctx, STATUS_NOT_FOUND, _KS("file not found"));
	}
	fp = kfiber_file_bind(fd);
	if (fp == NULL) {
		kfclose(fd);
		return out->f->error(out->ctx, STATUS_NOT_FOUND, _KS("file not found"));
	}
	//处理content-type
	char* content_type = find_content_type(rq, obj);
	if (content_type == NULL) {
		return out->f->error(out->ctx, STATUS_FORBIDEN, _KS("cann't find such content-type"));
	}
	defer(free(content_type));
	bool may_compress = false;
	if (obj->need_compress) {
		if (KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_GZIP)) {
			may_compress = true;
#ifdef ENABLE_BROTLI
		} else if (KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_BR)) {
			may_compress = true;
#endif
		}
	}
	if (may_compress && rq->file->get_file_size() >= conf.min_compress_length) {
		//如果可能压缩，则不回应206
		range = nullptr;
	}
	int status_code = STATUS_OK;
	int64_t left_send = (int64_t)rq->file->get_file_size();
	if (range) {
		//处理部分数据请求
		out->f->write_header(out->ctx, kgl_header_content_range, (char *)&left_send, KGL_HEADER_VALUE_INT64);
		if (!rq->sink->adjust_range(&left_send)) {
			return out->f->error(out->ctx, 416, _KS("range error"));
		}
		if (kfiber_file_seek(fp, seekBegin, range->from)) {
			return out->f->error(out->ctx, STATUS_SERVER_ERROR, _KS("cann't seek to right position"));
		}
		if (!KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
			KStringBuf b;
			char buf[INT2STRING_LEN];
			b.WSTR("bytes ");
			b << int2string(range->from, buf);
			b.WSTR("-");
			b << int2string(range->to, buf);
			b.WSTR("/");
			b << int2string(rq->file->get_file_size(), buf);
			out->f->write_header(out->ctx, kgl_header_content_range, b.buf(), b.size());
			status_code = STATUS_CONTENT_PARTIAL;
		} else {
			//url range的本地不缓存
			KBIT_SET(obj->index.flags, ANSW_NO_CACHE);
		}
	}
	out->f->write_status(out->ctx, status_code);
	rq->ctx.left_read = left_send;
	if (obj->never_compress) {
		out->f->write_header(out->ctx, kgl_header_content_encoding, kgl_expand_string("identity"));
	}
	//out->f->write_header(out->ctx, kgl_header_content_length, (const char*)&left_send, KGL_HEADER_VALUE_INT64);
	out->f->write_header(out->ctx, kgl_header_last_modified, (const char*)&last_modified, KGL_HEADER_VALUE_TIME);
	out->f->write_header(out->ctx, kgl_header_content_type, content_type, (hlen_t)strlen(content_type));
	//rq->buffer << "1234";
	//通知http头已经处理完成
	kgl_response_body body = { 0 };
	KGL_RESULT result = out->f->write_header_finish(out->ctx, left_send, &body);
	if (result != KGL_OK) {
		assert(body.ctx == nullptr);
		return result;
	}
	assert(body.f && body.ctx);
	if (body.f->support_sendfile(body.ctx)) {
		return body.f->close(body.ctx, body.f->sendfile(body.ctx, (KASYNC_FILE)fp, &rq->ctx.left_read));
	}
	if (!kasync_file_direct(fp, true)) {
		klog(KLOG_ERR, "cann't turn on file direct_on\n");
		return body.f->close(body.ctx, KGL_EIO);
	}
	int buf_size = conf.io_buffer;
	char* buf = (char*)aio_alloc_buffer(buf_size);
	if (buf == NULL) {
		return body.f->close(body.ctx, KGL_ENO_MEMORY);
	}
	while (rq->ctx.left_read > 0) {
		int len = (int)KGL_MIN(rq->ctx.left_read, (INT64)buf_size);
		int read_len = kfiber_file_read(fp, buf, len);
		if (read_len <= 0) {
			result = KGL_EIO;
			break;
		}
		result = body.f->write(body.ctx, kfiber_file_adjust(fp, buf), read_len);
		if (result != KGL_OK) {
			break;
		}
		rq->ctx.left_read -= read_len;
	}
	aio_free_buffer(buf);
	return body.f->close(body.ctx, result);
}
