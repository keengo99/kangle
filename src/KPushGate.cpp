#include "KPushGate.h"
#include "http.h"
#include "ksapi.h"
#include "KDechunkEngine.h"
#include "KBufferFetchObject.h"
#include "KVectorBufferFetchObject.h"
#include "HttpFiber.h"


struct kgl_dechunk_stream : public kgl_forward_stream
{
	char* saved_buffer;
	int saved_len;
	KDechunkEngine engine;
};

static KGL_RESULT dechunk_push_body(kgl_output_stream* gate, KREQUEST r, const char* buf, int len) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	KGL_RESULT result = STREAM_WRITE_SUCCESS;
	char* alloced_buffer = nullptr;
	if (g->saved_buffer) {
		alloced_buffer = (char*)malloc(len + g->saved_len);
		memcpy(alloced_buffer, g->saved_buffer, g->saved_len);
		memcpy(alloced_buffer + g->saved_len, buf, len);
		free(g->saved_buffer);
		g->saved_buffer = nullptr;
		buf = alloced_buffer;
		len += g->saved_len;
	}
	const char* piece;
	const char* end = buf + len;
	while (buf < end) {
		int piece_length = KHTTPD_MAX_CHUNK_SIZE;
		KDechunkResult status = g->engine.dechunk(&buf, end, &piece, &piece_length);
		switch (status) {
		case KDechunkResult::Trailer:
		{
#if 0
			fprintf(stderr, "trailer[");
			fwrite(piece, 1, piece_length, stderr);
			fprintf(stderr, "]\n");
#endif
			const char* trailer_end = piece + piece_length;
			const char* sp = (char*)memchr(piece, ':', piece_length);
			if (sp == nullptr) {
				continue;
			}
			hlen_t attr_len = (hlen_t)(sp - piece);
			sp++;
			while (sp < trailer_end && isspace((unsigned char)*sp)) {
				sp++;
			}
			result = g->down_stream->f->write_trailer(g->down_stream, r, piece, attr_len, sp, (hlen_t)(trailer_end - sp));
			if (result != KGL_OK) {
				goto done;
			}
			continue;
		}
		case KDechunkResult::Success:
		{
			assert(piece && piece_length > 0);
			KGL_RESULT result = g->down_stream->f->write_body(g->down_stream, r, piece, piece_length);
			if (result != KGL_OK) {
				goto done;
			}
			break;
		}
		case KDechunkResult::End: {
			result = KGL_END;
			goto done;
		}
		case KDechunkResult::Continue: {
			if (buf != end) {
				assert(g->saved_buffer == nullptr);
				g->saved_len = (int)(end - buf);
				g->saved_buffer = (char*)malloc(g->saved_len);
				memcpy(g->saved_buffer, buf, g->saved_len);
			}
			goto done;
		}
		default: {
			result = STREAM_WRITE_FAILED;
			goto done;
		}
		}
	}
done:
	if (alloced_buffer) {
		xfree(alloced_buffer);
	}
	return result;
}
KGL_RESULT final_sendfile(kgl_output_stream* out, KREQUEST r, KASYNC_FILE fp, int64_t *len) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->st) {
		return rq->ctx->st->sendfile(r, (kfiber_file*)fp, len);
	}
	return KGL_ENOT_SUPPORT;
}
bool final_support_sendfile(kgl_output_stream* out, KREQUEST r) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->st) {
		return rq->ctx->st->support_sendfile(r);
	}
	return rq->sink->support_sendfile();
}
bool support_sendfile_false(kgl_output_stream* out, KREQUEST r) {
	return false;
}
bool forward_support_sendfile(kgl_output_stream* out, KREQUEST r) {
	kgl_forward_stream* g = (kgl_forward_stream*)out;
	return g->down_stream->f->support_sendfile(out, r);
}
KGL_RESULT unsupport_sendfile(kgl_output_stream* out, KREQUEST r, KASYNC_FILE fp, int64_t *len) {
	assert(false);
	return KGL_ENOT_SUPPORT;
}
KGL_RESULT forward_sendfile(kgl_output_stream* out, KREQUEST r, KASYNC_FILE fp, int64_t *len) {
	kgl_forward_stream* g = (kgl_forward_stream*)out;
	return g->down_stream->f->sendfile(out, r, fp, len);
}
KGL_RESULT forward_write_header(kgl_output_stream* gate, KREQUEST r, kgl_header_type attr, const char* val, int val_len) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_header(g->down_stream, r, attr, val, val_len);
}
KGL_RESULT forward_write_unknow_header(kgl_output_stream* gate, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_unknow_header(g->down_stream, r, attr, attr_len, val, val_len);
}
KGL_RESULT forward_write_trailer(kgl_output_stream* gate, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_trailer(g->down_stream, r, attr, attr_len, val, val_len);
}
void forward_write_status(kgl_output_stream* gate, KREQUEST r, uint16_t status_code) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_status(g->down_stream, r, status_code);
}
KGL_RESULT forward_write_body(kgl_output_stream* gate, KREQUEST r, const char* buf, int len) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_body(g->down_stream, r, buf, len);
}
KGL_RESULT forward_write_header_finish(kgl_output_stream* gate, KREQUEST r) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_header_finish(g->down_stream, r);
}
KGL_RESULT forward_write_message(kgl_output_stream* gate, KREQUEST rq, KGL_MSG_TYPE type, const void* msg, int32_t msg_flag) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	return g->down_stream->f->write_message(g->down_stream, rq, type, msg, msg_flag);
}
KGL_RESULT forward_write_end(kgl_output_stream* out, KREQUEST rq, KGL_RESULT result) {
	kgl_forward_stream* g = (kgl_forward_stream*)out;
	return g->down_stream->f->write_end(g->down_stream, rq, result);
}
void forward_release(kgl_output_stream* gate) {
	kgl_forward_stream* g = (kgl_forward_stream*)gate;
	g->down_stream->f->release(g->down_stream);
	delete g;
}
static void dechunk_release(kgl_output_stream* gate) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	if (g->saved_buffer) {
		free(g->saved_buffer);
	}
	delete g;
}
static KGL_RESULT dechunk_write_end(kgl_output_stream* gate, KREQUEST rq, KGL_RESULT result) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	if (g->engine.is_success()) {
		return g->down_stream->f->write_end(g->down_stream, rq, KGL_OK);
	}
	return g->down_stream->f->write_end(g->down_stream, rq, KGL_EDATA_FORMAT);
}
static kgl_output_stream_function dechunk_stream_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_write_header_finish,	
	dechunk_push_body,
	forward_write_message,
	forward_write_trailer,
	support_sendfile_false,
	unsupport_sendfile,
	dechunk_write_end,
	dechunk_release
};
kgl_output_stream* new_dechunk_stream(kgl_output_stream* down_stream) {
	kgl_dechunk_stream* gate = new kgl_dechunk_stream;
	gate->f = &dechunk_stream_function;
	gate->down_stream = down_stream;
	gate->saved_buffer = nullptr;
	return gate;
}
kgl_output_stream_function forward_stream_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_write_header_finish,	
	forward_write_body,
	forward_write_message,
	forward_write_trailer,
	forward_support_sendfile,
	forward_sendfile,
	forward_write_end,
	forward_release
};
kgl_forward_stream* new_forward_stream(kgl_output_stream* down_stream) {
	kgl_forward_stream* gate = new kgl_forward_stream;
	gate->f = &forward_stream_function;
	gate->down_stream = down_stream;
	return gate;
}

struct kgl_default_output_stream : public kgl_output_stream
{
	KHttpResponseParser parser_ctx;
#if 0
	//rq->ctx->st 要放这里来。
	KWriteStream* st;
#endif
};

void st_write_status(kgl_output_stream* st, KREQUEST r, uint16_t status_code) {
	((KHttpRequest*)r)->ctx->obj->data->i.status_code = status_code;
}
KGL_RESULT st_write_header(kgl_output_stream* st, KREQUEST r, kgl_header_type attr, const char* val, int val_len) {
	kgl_default_output_stream* g = (kgl_default_output_stream*)st;
	KHttpRequest* rq = (KHttpRequest*)r;
	if (g->parser_ctx.parse_header((KHttpRequest*)rq, attr, val, val_len)) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT st_write_unknow_header(kgl_output_stream* st, KREQUEST rq, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_default_output_stream* g = (kgl_default_output_stream*)st;
	if (g->parser_ctx.parse_unknow_header((KHttpRequest*)rq, attr, attr_len, val, val_len, false)) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT st_write_header_finish(kgl_output_stream* st, KREQUEST r) {
	kgl_default_output_stream* g = (kgl_default_output_stream*)st;
	KHttpRequest* rq = (KHttpRequest*)r;
	g->parser_ctx.end_parse(rq);
	return on_upstream_finished_header(rq);
}

KGL_RESULT st_write_body(kgl_output_stream* st, KREQUEST r, const char* buf, int len) {
	KHttpRequest* rq = (KHttpRequest*)r;
	kassert(rq->ctx->st);
	return rq->ctx->st->write_all(rq, buf, len);
}
KGL_RESULT st_write_message(kgl_output_stream* st, KREQUEST r, KGL_MSG_TYPE msg_type, const void* msg, int msg_flag) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (msg_type == KGL_MSG_ERROR) {
		return handle_error(rq, msg_flag, (const char*)msg);
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	WSABUF* buf = (WSABUF*)msg;
	int64_t len;
	switch (msg_type) {
	case KGL_MSG_RAW:
	{
		len = (int64_t)msg_flag;
		break;
	}
	case KGL_MSG_VECTOR:
	{
		len = 0;
		for (int i = 0; i < msg_flag; i++) {
			len += buf[i].iov_len;
		}
		break;
	}
	default:
		len = -1;
		break;
	}
	if (len < 0) {
		return KGL_EINVALID_PARAMETER;
	}
	if (rq->sink->data.status_code == 0) {
		rq->response_status(200);
	}
	rq->response_content_length(len);
	rq->response_connection();
	rq->start_response_body(len);
	switch (msg_type) {
	case KGL_MSG_RAW:
		return rq->write_all((char*)msg, (int)len);
	case KGL_MSG_VECTOR:
		return rq->write_all(buf, msg_flag);
	default:
		return KGL_ENOT_SUPPORT;
	}
}
KGL_RESULT common_write_trailer(kgl_output_stream* st, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->st) {
		KGL_RESULT result = rq->ctx->st->flush(rq);
		if (result != KGL_OK) {
			return result;
		}
	}
	if (!rq->sink->response_trailer(attr, attr_len, val, val_len)) {
		return KGL_ESOCKET_BROKEN;
	}
	return KGL_OK;
}
static KGL_RESULT st_write_end(kgl_output_stream* st, KREQUEST r, KGL_RESULT result) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (result == KGL_OK && rq->ctx->left_read > 0) {
		//有content-length，又未读完
		result = KGL_ESOCKET_BROKEN;
	}
	if (rq->ctx->st) {
		return rq->ctx->st->write_end(rq, result);
	}
	return result;
}
void st_release(kgl_output_stream* st) {
	kgl_default_output_stream* g = (kgl_default_output_stream*)st;
	delete g;
}
static kgl_output_stream_function default_stream_function = {
	st_write_status,
	st_write_header,
	st_write_unknow_header,
	st_write_header_finish,
	st_write_body,
	st_write_message,
	common_write_trailer,
	final_support_sendfile,
	final_sendfile,
	st_write_end,
	st_release
};
kgl_output_stream* new_default_output_stream() {
	kgl_default_output_stream* st = new kgl_default_output_stream;
	st->f = &default_stream_function;
	return st;
}
static int64_t default_input_get_read_left(kgl_input_stream* st, KREQUEST r) {
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->sink->data.left_read;
}
static int default_input_read(kgl_input_stream* st, KREQUEST r, char* buf, int len) {
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->Read(buf, len);
}
static void default_input_release(kgl_input_stream* st) {

}
static kgl_input_stream_function default_input_stream_function = {
	default_input_get_read_left,
	default_input_read,
	default_input_release
};
static kgl_input_stream default_input_stream = {
	&default_input_stream_function
};

kgl_input_stream* new_default_input_stream() {
	return &default_input_stream;
}
void check_release(kgl_output_stream* out) {

}
void check_write_status(kgl_output_stream* st, KREQUEST r, uint16_t status_code) {
	((KHttpRequest*)r)->response_status(status_code);
}
KGL_RESULT check_write_header(kgl_output_stream* st, KREQUEST r, kgl_header_type attr, const char* val, int val_len) {
	KHttpRequest* rq = (KHttpRequest*)r;

	switch (attr) {
	case  kgl_header_content_length:
	{
		INT64 content_length;
		if (0 != kgl_parse_value_int64(val, val_len, &content_length)) {
			return KGL_ENOT_SUPPORT;
		}
		if (rq->sink->data.meth == METH_HEAD) {
			rq->ctx->left_read = 0;
		} else {
			rq->ctx->left_read = content_length;
		}
		return rq->response_content_length(content_length) ? KGL_OK : KGL_EINVALID_PARAMETER;
	}
	case kgl_header_connection:
	{
		rq->parse_connection(val, val + val_len);
		if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
			rq->ctx->left_read = -1;
		}
		rq->response_connection();
		return KGL_OK;
	}
	default:
		return rq->response_header(attr, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
	}
}
KGL_RESULT check_write_unknow_header(kgl_output_stream* st, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	return ((KHttpRequest*)r)->response_header(attr, attr_len, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
}

KGL_RESULT check_write_header_finish(kgl_output_stream* st, KREQUEST r) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (!rq->start_response_body(rq->ctx->left_read)) {
		return  KGL_EINVALID_PARAMETER;
	}
	if (rq->sink->data.meth == METH_HEAD || is_status_code_no_body(rq->sink->data.status_code)) {
		return KGL_NO_BODY;
	}
	return KGL_OK;
}
KGL_RESULT check_write_body(kgl_output_stream* st, KREQUEST r, const char* buf, int len) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->left_read > 0) {
		assert(rq->ctx->left_read >= len);
		rq->ctx->left_read -= len;
	}
	return rq->write_all(buf, len);
}
static KGL_RESULT check_write_end(kgl_output_stream* st, KREQUEST r, KGL_RESULT result) {
	KHttpRequest* rq = (KHttpRequest*)r;
	if (result == KGL_OK) {
		if (rq->ctx->left_read > 0) {
			//有content-length，又未读完
			return KGL_ESOCKET_BROKEN;
		}
	}
	return result;
}
static kgl_output_stream_function check_stream_function = {
	check_write_status,
	check_write_header,
	check_write_unknow_header,
	check_write_header_finish,
	check_write_body,
	st_write_message,
	common_write_trailer,
	final_support_sendfile,
	final_sendfile,	
	check_write_end,
	check_release
};
static kgl_output_stream check_output_stream = {
	&check_stream_function
};
kgl_output_stream* get_check_output_stream() {
	return &check_output_stream;
}
kgl_input_stream* get_check_input_stream() {
	return &default_input_stream;
}