#include "KPushGate.h"
#include "http.h"
#include "ksapi.h"
#include "KDechunkEngine.h"
#include "KBufferFetchObject.h"
#include "KVectorBufferFetchObject.h"
#include "HttpFiber.h"
#include "KHttpTransfer.h"

struct kgl_dechunk_stream : public kgl_forward_output_stream
{
	kgl_response_body body;
	char* saved_buffer;
	int saved_len;
	KDechunkEngine engine;
};

static KGL_RESULT dechunk_push_body(kgl_response_body_ctx* gate, const char* buf, int len) {
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
			result = g->down_stream.f->write_trailer(g->down_stream.ctx, piece, attr_len, sp, (hlen_t)(trailer_end - sp));
			if (result != KGL_OK) {
				goto done;
			}
			continue;
		}
		case KDechunkResult::Success:
		{
			assert(piece && piece_length > 0);
			KGL_RESULT result = g->body.f->write(g->body.ctx, piece, piece_length);
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

bool support_sendfile_false(kgl_response_body_ctx* out) {
	return false;
}
KGL_RESULT kgl_empty_flush(kgl_response_body_ctx* out) {
	return KGL_OK;
}
KGL_RESULT forward_flush(kgl_response_body_ctx* out) {
	kgl_forward_body* g = (kgl_forward_body*)out;
	return g->down_body.f->flush(g->down_body.ctx);
}
bool forward_support_sendfile(kgl_response_body_ctx* out) {
	kgl_forward_body* g = (kgl_forward_body*)out;
	return g->down_body.f->support_sendfile(g->down_body.ctx);
}
KGL_RESULT unsupport_sendfile(kgl_response_body_ctx* out, KASYNC_FILE fp, int64_t* len) {
	assert(false);
	return KGL_ENOT_SUPPORT;
}
KGL_RESULT forward_sendfile(kgl_response_body_ctx* out, KASYNC_FILE fp, int64_t* len) {
	kgl_forward_body* g = (kgl_forward_body*)out;
	return g->down_body.f->sendfile(g->down_body.ctx, fp, len);
}
KGL_RESULT forward_write_header(kgl_output_stream_ctx* gate, kgl_header_type attr, const char* val, int val_len) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->write_header(g->down_stream.ctx, attr, val, val_len);
}
KGL_RESULT forward_write_unknow_header(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->write_unknow_header(g->down_stream.ctx, attr, attr_len, val, val_len);
}
KGL_RESULT forward_write_trailer(kgl_output_stream_ctx* gate, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->write_trailer(g->down_stream.ctx, attr, attr_len, val, val_len);
}
void forward_write_status(kgl_output_stream_ctx* gate, uint16_t status_code) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->write_status(g->down_stream.ctx, status_code);
}
KGL_RESULT forward_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc) {
	kgl_forward_body* g = (kgl_forward_body*)ctx;
	return g->down_body.f->writev(g->down_body.ctx, bufs, bc);
}
KGL_RESULT forward_write(kgl_response_body_ctx* gate, const char* buf, int len) {
	kgl_forward_body* g = (kgl_forward_body*)gate;
	return g->down_body.f->write(g->down_body.ctx, buf, len);
}
KGL_RESULT forward_write_header_finish(kgl_output_stream_ctx* gate, int64_t body_size, kgl_response_body* body) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->write_header_finish(g->down_stream.ctx,body_size, body);
}
KGL_RESULT forward_error(kgl_output_stream_ctx* gate, uint16_t status_code, const char* msg, size_t msg_len) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	return g->down_stream.f->error(g->down_stream.ctx, status_code, msg, msg_len);
}
KGL_RESULT forward_close(kgl_response_body_ctx* out, KGL_RESULT result) {
	kgl_forward_body* g = (kgl_forward_body*)out;
	return g->down_body.f->close(g->down_body.ctx, result);
}
void forward_release(kgl_output_stream_ctx* gate) {
	kgl_forward_output_stream* g = (kgl_forward_output_stream*)gate;
	g->down_stream.f->close(g->down_stream.ctx);
	delete g;
}

static void dechunk_release(kgl_output_stream_ctx* gate) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	if (g->saved_buffer) {
		free(g->saved_buffer);
	}
	delete g;
}
static KGL_RESULT dechunk_close(kgl_response_body_ctx* gate, KGL_RESULT result) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	if (g->engine.is_success()) {
		return g->body.f->close(g->body.ctx, result);
	}
	return g->body.f->close(g->body.ctx, KGL_EDATA_FORMAT);
}
static kgl_response_body_function dechunk_body_function = {
	unsupport_writev<dechunk_push_body>,
	dechunk_push_body,
	kgl_empty_flush,
	support_sendfile_false,
	unsupport_sendfile,
	dechunk_close
};

KGL_RESULT dechunk_header_finish(kgl_output_stream_ctx* gate, int64_t body_size, kgl_response_body* body) {
	kgl_dechunk_stream* g = (kgl_dechunk_stream*)gate;
	g->body = { 0 };
	KGL_RESULT result = g->down_stream.f->write_header_finish(g->down_stream.ctx, -1, &g->body);
	if (g->body.ctx) {
		body->ctx = (kgl_response_body_ctx*)g;
		body->f = &dechunk_body_function;
	}
	return result;
}
static kgl_output_stream_function dechunk_stream_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_error,
	dechunk_header_finish,
	forward_write_trailer,
	dechunk_release
};


bool new_dechunk_stream(kgl_output_stream* down_stream) {
	kgl_dechunk_stream* gate = new kgl_dechunk_stream;
	gate->down_stream = *down_stream;
	down_stream->f = &dechunk_stream_function;
	down_stream->ctx = (kgl_output_stream_ctx*)gate;
	gate->saved_buffer = nullptr;
	return true;
}
kgl_output_stream_function forward_stream_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_error,
	forward_write_header_finish,
	forward_write_trailer,
	forward_release
};
kgl_response_body_function forward_body_function = {
	forward_writev,
	forward_write,
	forward_flush,
	forward_support_sendfile,
	forward_sendfile,
	forward_close
};


void st_write_status(kgl_output_stream_ctx* st, uint16_t status_code) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	g->rq->ctx.obj->data->i.status_code = status_code;
}
KGL_RESULT st_write_header(kgl_output_stream_ctx* st, kgl_header_type attr, const char* val, int val_len) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	if (g->parser_ctx.parse_header(g->rq, attr, val, val_len)) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT st_write_unknow_header(kgl_output_stream_ctx* st, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	if (g->parser_ctx.parse_unknow_header(g->rq, attr, attr_len, val, val_len, false)) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT st_write_header_finish(kgl_output_stream_ctx* st,int64_t body_size, kgl_response_body* body) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	g->parser_ctx.end_parse(g->rq, body_size);
	return on_upstream_finished_header(g->rq, body);
#if 0
	if (result != KGL_OK) {
		return result;
	}

	if (!g->rq->ctx.st.ctx) {
		get_default_response_body(g->rq, &g->rq->ctx.st);
		if (!kgl_load_response_body(g->rq, &g->rq->ctx.st)) {
			g->rq->ctx.st.f->close(g->rq->ctx.st.ctx, KGL_ENOT_PREPARE);
			return KGL_ENOT_PREPARE;
		}
		*body = g->rq->ctx.st;
	}
	return KGL_OK;
#endif
}
KGL_RESULT st_write_message(kgl_output_stream_ctx* st, uint16_t status_code, const char* msg, size_t msg_len) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	KHttpRequest* rq = g->rq;
	return handle_error(rq, status_code, (const char*)msg);
#if 0
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
#endif
}
KGL_RESULT common_write_trailer(KHttpRequest* rq, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	if (rq->ctx.body.ctx) {
		KGL_RESULT result = rq->ctx.body.f->flush(rq->ctx.body.ctx);
		if (result != KGL_OK) {
			return result;
		}
	}
	if (!rq->sink->response_trailer(attr, attr_len, val, val_len)) {
		return KGL_ESOCKET_BROKEN;
	}
	return KGL_OK;
}
KGL_RESULT check_write_trailer(kgl_output_stream_ctx* st, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	return common_write_trailer((KHttpRequest*)st, attr, attr_len, val, val_len);
}
KGL_RESULT st_write_trailer(kgl_output_stream_ctx* st, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	return common_write_trailer(g->rq, attr, attr_len, val, val_len);
}
void st_release(kgl_output_stream_ctx* st) {
	kgl_default_output_stream_ctx* g = (kgl_default_output_stream_ctx*)st;
	delete g;
}
static kgl_output_stream_function default_stream_function = {
	st_write_status,
	st_write_header,
	st_write_unknow_header,
	st_write_message,
	st_write_header_finish,
	st_write_trailer,
	st_release,
};
void new_default_output_stream(KHttpRequest* rq, kgl_output_stream* out) {
	kgl_default_output_stream_ctx* st = new kgl_default_output_stream_ctx;
	out->f = &default_stream_function;
	st->rq = rq;
	out->ctx = (kgl_output_stream_ctx*)st;
	return;
}
void new_default_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) 	{
	new_default_input_stream(rq, in);
	new_default_output_stream(rq, out);
}
static int64_t default_input_get_read_left(kgl_request_body_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->sink->data.left_read;
}
static int default_input_read(kgl_request_body_ctx* ctx, char* buf, int len) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->read(buf, len);
}
static int64_t default_input_has_filter_get_left(kgl_request_body_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	assert(rq->ctx.in_body);
	return rq->ctx.in_body->f->get_left(rq->ctx.in_body->ctx);
}
static int default_input_has_filter_read(kgl_request_body_ctx* ctx, char* buf, int len) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	assert(rq->ctx.in_body);
	return rq->ctx.in_body->f->read(rq->ctx.in_body->ctx,buf,len);
}
static int default_get_header_count(kgl_input_stream_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	int count = 0;
	KHttpHeader* header = rq->sink->data.get_header();
	while (header) {
		if (!is_internal_header(header)) {
			count++;
		}
		header = header->next;
	}
	return count;
}
static kgl_url* default_get_url(kgl_input_stream_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	KUrl* url = rq->sink->data.url;
	if (KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_RAW_URL) || !KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_REWRITED)) {
		url = rq->sink->data.raw_url;
	}
	return url;
}
KGL_RESULT default_get_header(kgl_input_stream_ctx* ctx, kgl_parse_header_ctx* parse_ctx, kgl_parse_header_function* parse) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	KGL_RESULT result = KGL_OK;
	KHttpHeader* header = rq->sink->data.get_header();
	while (header) {
		if (!is_internal_header(header)) {
			if (header->name_is_know) {
				result = parse->parse_header(parse_ctx, (kgl_header_type)header->know_header, header->buf + header->val_offset, header->val_len);
			} else {
				result = parse->parse_unknow_header(parse_ctx, header->buf, header->name_len, header->buf + header->val_offset, header->val_len);
			}
			if (result != KGL_OK) {
				return result;
			}
		}
		header = header->next;
	}	
	return result;
}
kgl_precondition *default_get_precondition(kgl_input_stream_ctx* ctx, kgl_precondition_flag* flag) {
	KHttpRequest* rq = (KHttpRequest*)ctx;	
	if (rq->ctx.sub_request) {
		*flag = (kgl_precondition_flag)rq->ctx.precondition_flag;
		return rq->ctx.sub_request->precondition;
	}
	return rq->sink->get_precondition(flag);
}
kgl_request_range *default_get_range(kgl_input_stream_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	if (rq->ctx.sub_request) {
		return rq->ctx.sub_request->range;
	}
	return rq->sink->data.range;
}
static void default_input_release(kgl_request_body_ctx* ctx) {
}
static void default_input_has_filter_release(kgl_request_body_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	assert(rq->ctx.in_body);
	if (rq->ctx.in_body) {
		rq->ctx.in_body->f->close(rq->ctx.in_body->ctx);
		rq->ctx.in_body = nullptr;
	}
}
static kgl_request_body_function default_request_body_function = {
	default_input_get_read_left,
	default_input_read,
	default_input_release,
};
void kgl_init_request_in_body(KHttpRequest* rq)
{
	rq->ctx.in_body = rq->sink->alloc<kgl_request_body>();
	rq->ctx.in_body->ctx = (kgl_request_body_ctx *)rq;
	rq->ctx.in_body->f = &default_request_body_function;
}
static kgl_input_stream_function default_input_stream_function = {	
	{default_request_body_function},
	default_get_url,
	default_get_precondition,
	default_get_range,
	default_get_header_count,
	default_get_header	
};
static kgl_input_stream_function default_has_filter_input_stream_function = {
	default_input_has_filter_get_left,
	default_input_has_filter_read,
	default_input_has_filter_release,
	default_get_url,
	default_get_precondition,
	default_get_range,
	default_get_header_count,
	default_get_header
};


void new_default_input_stream(KHttpRequest* rq, kgl_input_stream* in) {
	in->ctx = (kgl_input_stream_ctx*)rq;
	if (rq->ctx.in_body) {
		in->f = &default_has_filter_input_stream_function;
	} else {
		in->f = &default_input_stream_function;
	}
}
void check_release(kgl_output_stream_ctx* out) {
}
void check_write_status(kgl_output_stream_ctx* st, uint16_t status_code) {
	((KHttpRequest*)st)->response_status(status_code);
}
KGL_RESULT check_write_header(kgl_output_stream_ctx* st, kgl_header_type attr, const char* val, int val_len) {
	KHttpRequest* rq = (KHttpRequest*)st;

	switch (attr) {
	case  kgl_header_content_length:
	{
#if 0
		INT64 content_length;
		if (0 != kgl_parse_value_int64(val, val_len, &content_length)) {
			return KGL_ENOT_SUPPORT;
		}
		if (rq->sink->data.meth == METH_HEAD) {
			rq->ctx.left_read = 0;
		} else {
			rq->ctx.left_read = content_length;
		}
		return rq->response_content_length(content_length) ? KGL_OK : KGL_EINVALID_PARAMETER;
#endif
		return KGL_OK;
	}
	case kgl_header_connection:
	{
		rq->parse_connection(val, val + val_len);
		if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
			rq->ctx.left_read = -1;
		}		
		return KGL_OK;
	}
	default:
		return rq->response_header(attr, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
	}
}
kgl_url* forward_get_url(kgl_input_stream_ctx* ctx) 	{
	kgl_forward_input_stream* st = (kgl_forward_input_stream*)ctx;
	return st->up_stream.f->get_url(st->up_stream.ctx);
}
int forward_get_header_count(kgl_input_stream_ctx* ctx) 	{
	kgl_forward_input_stream* st = (kgl_forward_input_stream*)ctx;
	return st->up_stream.f->get_header_count(st->up_stream.ctx);
}
kgl_precondition *forward_get_precondition(kgl_input_stream_ctx* ctx, kgl_precondition_flag *flag) 	{
	kgl_forward_input_stream* st = (kgl_forward_input_stream*)ctx;
	return st->up_stream.f->get_precondition(st->up_stream.ctx, flag);
}
kgl_request_range *forward_get_range(kgl_input_stream_ctx* ctx) 	{
	kgl_forward_input_stream* st = (kgl_forward_input_stream*)ctx;
	return st->up_stream.f->get_range(st->up_stream.ctx);
}
KGL_RESULT forward_get_header(kgl_input_stream_ctx* ctx, kgl_parse_header_ctx* cb_ctx, kgl_parse_header_function* cb) {
	kgl_forward_input_stream* st = (kgl_forward_input_stream*)ctx;
	return st->up_stream.f->get_header(st->up_stream.ctx, cb_ctx, cb);
}
KGL_RESULT check_write_unknow_header(kgl_output_stream_ctx* st, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len) {
	return ((KHttpRequest*)st)->response_header(attr, attr_len, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
}
static KGL_RESULT default_writev(kgl_response_body_ctx* ctx, WSABUF* bufs, int bc) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->write_all(bufs, bc);
}
static KGL_RESULT default_write(kgl_response_body_ctx* ctx, const char* buf, int size) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->write_all(buf, size);
}
static KGL_RESULT default_flush(kgl_response_body_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	rq->sink->flush();
	return KGL_OK;
}
static bool default_support_sendfile(kgl_response_body_ctx* ctx) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->sink->support_sendfile();
}
static 	KGL_RESULT default_sendfile(kgl_response_body_ctx* ctx, KASYNC_FILE fp, int64_t* length) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->sendfile(fp, length);
}
static KGL_RESULT default_close(kgl_response_body_ctx* ctx, KGL_RESULT result) {
	KHttpRequest* rq = (KHttpRequest*)ctx;
	return rq->write_end(result);
}
static kgl_response_body_function kgl_default_response_body = {
	default_writev,
	default_write,
	default_flush,
	default_support_sendfile,
	default_sendfile,
	default_close
};
void get_default_response_body(KREQUEST r, kgl_response_body* body) {
	body->ctx = (kgl_response_body_ctx*)r;
	body->f = &kgl_default_response_body;
}
KGL_RESULT check_write_header_finish(kgl_output_stream_ctx* st, int64_t body_size, kgl_response_body* body) {
	KHttpRequest* rq = (KHttpRequest*)st;
	rq->response_content_length(body_size);
	rq->response_connection();
	if (!rq->start_response_body(body_size)) {
		return  KGL_EINVALID_PARAMETER;
	}
	if (rq->sink->data.meth == METH_HEAD || is_status_code_no_body(rq->sink->data.status_code)) {
		return KGL_NO_BODY;
	}
	assert(rq->ctx.body.ctx == nullptr);
	get_default_response_body(rq, &rq->ctx.body);
	*body = rq->ctx.body;
	return KGL_OK;
}
static kgl_output_stream_function check_stream_function = {
	check_write_status,
	check_write_header,
	check_write_unknow_header,
	st_write_message,
	check_write_header_finish,
	check_write_trailer,
	check_release
};
void get_check_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	in->ctx = (kgl_input_stream_ctx*)rq;
	in->f = &default_input_stream_function;
	out->ctx = (kgl_output_stream_ctx*)rq;
	out->f = &check_stream_function;
}
void get_check_output_stream(KHttpRequest* rq, kgl_output_stream* out) 	{
	out->ctx = (kgl_output_stream_ctx*)rq;
	out->f = &check_stream_function;
}
void pipe_response_body(kgl_forward_body* forward_body, kgl_response_body_function* f, kgl_response_body* down_body) {
	forward_body->down_body = *down_body;
	down_body->ctx = (kgl_response_body_ctx*)forward_body;
	down_body->f = f;
}
void pipe_output_stream(kgl_forward_output_stream* forward_st, kgl_output_stream_function* f, kgl_output_stream* down_stream) {
	forward_st->down_stream = *down_stream;
	down_stream->ctx = (kgl_output_stream_ctx*)forward_st;
	down_stream->f = f;
}
void pipe_input_stream(kgl_forward_input_stream* forward_st, kgl_input_stream_function* f, kgl_input_stream* up_stream) {
	forward_st->up_stream = *up_stream;
	up_stream->ctx = (kgl_input_stream_ctx*)forward_st;
	up_stream->f = f;
}

KGL_RESULT kgl_write_buf(kgl_response_body* body, kbuf* buf, int length) {

#define KGL_RQ_WRITE_BUF_COUNT 16
	WSABUF bufs[KGL_RQ_WRITE_BUF_COUNT];
	while (buf) {
		int bc = 0;
		while (bc < KGL_RQ_WRITE_BUF_COUNT && buf) {
			if (length == 0) {
				break;
			}
			if (length > 0) {
				bufs[bc].iov_len = KGL_MIN(length, buf->used);
				length -= bufs[bc].iov_len;
			} else {
				bufs[bc].iov_len = buf->used;
			}
			bufs[bc].iov_base = buf->data;
			buf = buf->next;
			bc++;
		}
		if (bc == 0) {
			if (length > 0) {
				return KGL_ENO_DATA;
			}
			assert(length == 0);
			return KGL_OK;
		}
		KGL_RESULT result = body->f->writev(body->ctx, bufs, bc);
		if (result != KGL_OK) {
			return result;
		}
	}
	return KGL_OK;
}