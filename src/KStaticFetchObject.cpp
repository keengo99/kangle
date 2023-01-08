#include "KStaticFetchObject.h"
#include "http.h"
#include "KContentType.h"
#include "KVirtualHostManage.h"
#include "kfiber.h"


KGL_RESULT KStaticFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{
	KGL_RESULT result = InternalProcess(rq, out);
	Close(rq);
	if (result == KGL_NO_BODY) {
		return result;
	}
	return  out->f->write_end(out, rq, result);
}
KGL_RESULT KStaticFetchObject::InternalProcess(KHttpRequest* rq, kgl_output_stream* out)
{
	kassert(!rq->file->isDirectory());
	KHttpObject* obj = rq->ctx->obj;
	KBIT_SET(obj->index.flags, ANSW_LOCAL_SERVER);
	if (rq->sink->data.if_modified_since > 0 && rq->sink->data.if_modified_since == rq->file->getLastModified()) {
		if (rq->ctx->mt == modified_if_modified) {
			out->f->write_status(out, rq, STATUS_NOT_MODIFIED);
			return out->f->write_header_finish(out, rq);
		}
	} else if (rq->ctx->mt == modified_if_range_date || rq->ctx->mt == modified_if_range_etag) {
		KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
	}
	assert(fp == NULL);
	assert(rq->file);
	int file_flag = KFILE_ASYNC;
	if (!KBIT_TEST(rq->sink->data.flags, RQ_HAVE_RANGE)) {
		KBIT_SET(file_flag, KFILE_SEQUENTIAL);
	}
	FILE_HANDLE fd = rq->file->open(fileRead, file_flag);
	if (!kflike(fd)) {
		return out->f->write_message(out, rq, KGL_MSG_ERROR, "file not found", STATUS_NOT_FOUND);
	}
	fp = kfiber_file_bind(fd, KFILE_ASYNC);
	if (fp == NULL) {
		kfclose(fd);
		return out->f->write_message(out, rq, KGL_MSG_ERROR, "file not found", STATUS_NOT_FOUND);
	}
	//处理content-type
	char* content_type = find_content_type(rq, obj);
	if (content_type == NULL) {
		return out->f->write_message(out, rq, KGL_MSG_ERROR, "cann't find such content-type", STATUS_FORBIDEN);
	}
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
		KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
	}
	int status_code = STATUS_OK;
	int64_t left_send = (int64_t)rq->file->get_file_size();
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAVE_RANGE)) {
		//处理部分数据请求
		rq->ctx->content_range_length = rq->file->get_file_size();
		if (!rq->sink->adjust_range(&left_send)) {
			xfree(content_type);
			return out->f->write_message(out, rq, KGL_MSG_ERROR, "range error", 416);
		}
		if (kfiber_file_seek(fp, seekBegin, rq->sink->data.range_from)) {
			xfree(content_type);
			return out->f->write_message(out, rq, KGL_MSG_ERROR, "cann't seek to right position", 500);
		}
		if (!KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
			KStringBuf b;
			char buf[INT2STRING_LEN];
			b.WSTR("bytes ");
			b << int2string(rq->sink->data.range_from, buf) << "-";
			b << int2string(rq->sink->data.range_to, buf) << "/";
			b << int2string(rq->file->get_file_size(), buf);
			out->f->write_header(out, rq, kgl_header_content_range, b.getString(), b.getSize());
			status_code = STATUS_CONTENT_PARTIAL;
		} else {
			//url range的本地不缓存
			KBIT_SET(obj->index.flags, ANSW_NO_CACHE);
		}
		if (!KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
			KStringBuf b;
			char buf[INT2STRING_LEN];
			b.WSTR("bytes ");
			b << int2string(rq->sink->data.range_from, buf) << "-";
			b << int2string(rq->sink->data.range_to, buf) << "/";
			b << int2string(rq->file->get_file_size(), buf);
			out->f->write_header(out, rq, kgl_header_content_range, b.getString(), b.getSize());
			status_code = STATUS_CONTENT_PARTIAL;
		} else {
			//url range的本地不缓存
			KBIT_SET(obj->index.flags, ANSW_NO_CACHE);
		}
	}
	out->f->write_status(out, rq, status_code);
	rq->ctx->left_read = left_send;
	if (obj->never_compress) {
		out->f->write_header(out, rq, kgl_header_content_encoding, kgl_expand_string("identity"));
	}
	out->f->write_header(out, rq, kgl_header_content_length, (const char*)&left_send, KGL_HEADER_VALUE_INT64);
	time_t last_modified = rq->file->getLastModified();
	out->f->write_header(out, rq, kgl_header_last_modified, (const char*)&last_modified, KGL_HEADER_VALUE_TIME);
	out->f->write_header(out, rq, kgl_header_content_type, content_type, (hlen_t)strlen(content_type));
	xfree(content_type);
	//rq->buffer << "1234";
	//通知http头已经处理完成
	KGL_RESULT result = out->f->write_header_finish(out, rq);
	if (result != KGL_OK) {
		return result;
	}
	if (out->f->support_sendfile(out, rq)) {
		return out->f->sendfile(out, rq, (KASYNC_FILE)fp, &rq->ctx->left_read);
	}
	int buf_size = conf.io_buffer;
	char* buf = (char*)aio_alloc_buffer(buf_size);
	if (buf == NULL) {
		return KGL_ENO_MEMORY;
	}
	while (rq->ctx->left_read > 0) {
		int len = (int)KGL_MIN(rq->ctx->left_read, (INT64)buf_size);
		int read_len = kfiber_file_read(fp, buf, len);
		if (read_len <= 0) {
			result = KGL_EIO;
			break;
		}
		result = out->f->write_body(out, rq, kfiber_file_adjust(fp, buf), read_len);
		if (result != KGL_OK) {
			break;
		}
		rq->ctx->left_read -= read_len;
	}
	aio_free_buffer(buf);
	return result;
}
