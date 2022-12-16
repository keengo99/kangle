#include "KPushGate.h"
#include "http.h"
#include "ksapi.h"
#include "KDechunkEngine.h"
#include "KBufferFetchObject.h"
#include "KVectorBufferFetchObject.h"
#include "HttpFiber.h"


struct kgl_dechunk_stream : public kgl_forward_stream {
	KDechunkEngine engine;
};


static KGL_RESULT forward_write_unknow_header(kgl_output_stream *gate, KREQUEST r, kgl_header_type attr, const char *val, hlen_t val_len)
{
	return gate->f->write_unknow_header(gate, r, kgl_header_type_string[attr].data, (hlen_t)kgl_header_type_string[attr].len, val, val_len);
}

static KGL_RESULT dechunk_push_body(kgl_output_stream*gate, KREQUEST r, const char *buf, int len)
{
	kgl_dechunk_stream *g = (kgl_dechunk_stream *)gate;
	const char *piece;
	int piece_length;
	while (len > 0) {
		piece_length = KHTTPD_MAX_CHUNK_SIZE;
		KDechunkResult status = g->engine.dechunk(&buf, len, &piece, piece_length);
		switch (status) {
		case KDechunkResult::Success:
		{
			assert(piece && piece_length > 0);
			KGL_RESULT result = g->down_stream->f->write_body(g->down_stream, r, piece, piece_length);
			if (result != KGL_OK) {
				return result;
			}
			break;
		}
		case KDechunkResult::End:
			return STREAM_WRITE_END;
		case KDechunkResult::Continue:
			assert(len == 0);
			return STREAM_WRITE_SUCCESS;
		default:			
			return STREAM_WRITE_FAILED;
		}
	}
	return STREAM_WRITE_SUCCESS;
}
KGL_RESULT forward_write_header(kgl_output_stream*gate, KREQUEST r, kgl_header_type attr, const char *val, hlen_t val_len)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_header(g->down_stream, r, attr,val,val_len);
}
KGL_RESULT forward_write_unknow_header(kgl_output_stream*gate, KREQUEST r, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_unknow_header(g->down_stream, r, attr, attr_len, val, val_len);
}
void forward_write_status(kgl_output_stream*gate, KREQUEST r, int status_code)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_status(g->down_stream, r, status_code);
}
KGL_RESULT forward_write_body(kgl_output_stream*gate, KREQUEST r, const char *buf, int len)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_body(g->down_stream, r, buf, len);
}
KGL_RESULT forward_write_header_finish(kgl_output_stream*gate, KREQUEST r)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_header_finish(g->down_stream, r);
}
KGL_RESULT forward_write_message(kgl_output_stream*gate, KREQUEST rq, KGL_MSG_TYPE type, const void *msg, int32_t msg_flag)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	return g->down_stream->f->write_message(g->down_stream, rq, type, msg, msg_flag);
}
KGL_RESULT forward_write_end(kgl_output_stream *out, KREQUEST rq, KGL_RESULT result)
{
	kgl_forward_stream *g = (kgl_forward_stream *)out;
	return g->down_stream->f->write_end(g->down_stream, rq, result);
}
void forward_release(kgl_output_stream*gate)
{
	kgl_forward_stream *g = (kgl_forward_stream *)gate;
	g->down_stream->f->release(g->down_stream);
	delete g;
}
static void dechunk_release(kgl_output_stream*gate)
{
	kgl_dechunk_stream *g = (kgl_dechunk_stream *)gate;
	delete g;
}
static KGL_RESULT dechunk_write_end(kgl_output_stream* gate, KREQUEST rq, KGL_RESULT result)
{
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
	dechunk_write_end,
	dechunk_release
};
kgl_output_stream*new_dechunk_stream(kgl_output_stream*down_stream)
{
	kgl_dechunk_stream*gate = new kgl_dechunk_stream;
	gate->f = &dechunk_stream_function;
	gate->down_stream = down_stream;
	return gate;
}
kgl_output_stream_function forward_stream_function = {
	forward_write_status,
	forward_write_header,
	forward_write_unknow_header,
	forward_write_header_finish,
	forward_write_body,
	forward_write_message,
	forward_write_end,
	forward_release
};
kgl_forward_stream* new_forward_stream(kgl_output_stream*down_stream)
{
	kgl_forward_stream *gate = new kgl_forward_stream;
	gate->f = &forward_stream_function;
	gate->down_stream = down_stream;
	return gate;
}


struct kgl_default_output_stream : public kgl_output_stream {
	KHttpResponseParser parser_ctx;
};


void st_write_status(kgl_output_stream*st, KREQUEST r, int status_code)
{
	((KHttpRequest *)r)->ctx->obj->data->status_code = status_code;
}
KGL_RESULT st_write_header(kgl_output_stream*st, KREQUEST r, kgl_header_type attr, const char *val, hlen_t val_len)
{
	kgl_default_output_stream *g = (kgl_default_output_stream *)st;
	KHttpRequest *rq = (KHttpRequest *)r;
	KHttpObject *obj = rq->ctx->obj;
	if (val_len == 0) {
		switch (attr) {
		case  kgl_header_content_length:
		{
			INT64 *content_length = (INT64*)val;
			kassert(*content_length >= 0);
			obj->index.content_length = *content_length;
			KBIT_SET(obj->index.flags, ANSW_HAS_CONTENT_LENGTH);
			return KGL_OK;
		}
		case kgl_header_last_modified:
		{
			obj->index.last_modified = *(time_t *)val;
			char tmp_buf[42];
			mk1123time(obj->index.last_modified, tmp_buf, 41);
			KBIT_SET(obj->index.flags, ANSW_LAST_MODIFIED);
			g->parser_ctx.AddHeader(kgl_header_type_string[attr].data, (hlen_t)kgl_header_type_string[attr].len, tmp_buf, 29);
			return KGL_OK;
		}
		default:
		{
			return KGL_ENOT_SUPPORT;
		}
		}
	}
	return forward_write_unknow_header(st, r, attr, val, val_len);
}
KGL_RESULT st_write_unknow_header(kgl_output_stream*st, KREQUEST rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	kgl_default_output_stream *g = (kgl_default_output_stream *)st;
	if (g->parser_ctx.ParseHeader((KHttpRequest *)rq, attr, attr_len, val, val_len, false)) {
		return KGL_OK;
	}
	return KGL_EDATA_FORMAT;
}
KGL_RESULT st_write_header_finish(kgl_output_stream*st, KREQUEST r)
{
	kgl_default_output_stream *g = (kgl_default_output_stream *)st;
	KHttpRequest *rq = (KHttpRequest *)r;
	g->parser_ctx.EndParse(rq);
	return on_upstream_finished_header(rq);
}

KGL_RESULT st_write_body(kgl_output_stream*st, KREQUEST r, const char *buf, int len)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	kassert(rq->ctx->st);
	return rq->ctx->st->write_all(rq, buf, len);
}
KGL_RESULT st_write_message(kgl_output_stream*st, KREQUEST r, KGL_MSG_TYPE msg_type, const void *msg, int msg_flag)
{
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
		break;
	}
	if (len < 0) {
		return KGL_EINVALID_PARAMETER;
	}
	if (rq->sink->data.status_code == 0) {
		rq->responseStatus(200);
	}
	rq->responseContentLength(len);
	rq->responseConnection();
	rq->startResponseBody(len);
	bool result = true;
	switch (msg_type) {
	case KGL_MSG_RAW:
		result = rq->WriteAll((char *)msg, (int)len);
		break;
	case KGL_MSG_VECTOR:
		for (int i = 0; i < msg_flag; i++) {
			if (!rq->WriteAll((char *)buf[0].iov_base, (int)buf[0].iov_len)) {
				result = false;
				break;
			}
		}
		break;
	default:
		break;
	}
	return result ? KGL_OK : KGL_ESOCKET_BROKEN;
}
static KGL_RESULT st_write_end(kgl_output_stream *st, KREQUEST r, KGL_RESULT result)
{
	KHttpRequest *rq = (KHttpRequest *)r;
	if (result==KGL_OK && rq->ctx->know_length && rq->ctx->left_read != 0) {
		//有content-length，又未读完
		result = KGL_ESOCKET_BROKEN;
	}
	if (rq->ctx->st) {
		return rq->ctx->st->write_end(rq, result);
	}
	return result;
}
void st_release(kgl_output_stream*st)
{
	kgl_default_output_stream *g = (kgl_default_output_stream *)st;
	delete g;
}
static kgl_output_stream_function default_stream_function = {
	st_write_status,
	st_write_header,
	st_write_unknow_header,
	st_write_header_finish,
	st_write_body,
	st_write_message,
	st_write_end,
	st_release
};

kgl_output_stream *new_default_output_stream()
{
	kgl_default_output_stream *st = new kgl_default_output_stream;
	st->f = &default_stream_function;
	return st;
}


static int64_t default_input_get_read_left(kgl_input_stream* st, KREQUEST r)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->sink->data.left_read;
}
static int default_input_read(kgl_input_stream* st, KREQUEST r, char* buf, int len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	return rq->Read(buf, len);
}
static void default_input_release(kgl_input_stream* st)
{

}
static kgl_input_stream_function default_input_stream_function = {
	default_input_get_read_left,
	default_input_read,
	default_input_release
};
static kgl_input_stream default_input_stream = { 
	&default_input_stream_function
};

kgl_input_stream* new_default_input_stream()
{
	return &default_input_stream;
}
void check_release(kgl_output_stream* out)
{

}
void check_write_status(kgl_output_stream* st, KREQUEST r, int status_code)
{
	((KHttpRequest*)r)->responseStatus(status_code);
}
KGL_RESULT check_write_header(kgl_output_stream* st, KREQUEST r, kgl_header_type attr, const char* val, hlen_t val_len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	if (val_len == 0) {
		switch (attr) {
		case  kgl_header_content_length:
		{
			INT64* content_length = (INT64*)val;
			rq->ctx->know_length = 1;
			rq->ctx->left_read = *content_length;
			return rq->responseContentLength(*content_length) ? KGL_OK : KGL_EINVALID_PARAMETER;
		}
		default:
			//NOT SUPPORT
			assert(false);
			return KGL_ENOT_SUPPORT;
		}
	}
	return rq->responseHeader(attr, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
}
KGL_RESULT check_write_unknow_header(kgl_output_stream* st, KREQUEST r, const char* attr, hlen_t attr_len, const char* val, hlen_t val_len)
{
	return ((KHttpRequest*)r)->responseHeader(attr, attr_len, val, val_len) ? KGL_OK : KGL_EINVALID_PARAMETER;
}
KGL_RESULT check_write_header_finish(kgl_output_stream* st, KREQUEST r)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->know_length) {
		return rq->startResponseBody(rq->ctx->left_read) ? KGL_OK : KGL_EINVALID_PARAMETER;
	}
	return rq->startResponseBody(-1) ? KGL_OK : KGL_EINVALID_PARAMETER;
}
KGL_RESULT check_write_body(kgl_output_stream* st, KREQUEST r, const char* buf, int len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	if (rq->ctx->know_length) {
		rq->ctx->left_read -= len;
	}
	return rq->WriteAll(buf, len)?KGL_OK:KGL_ESOCKET_BROKEN;
}
static KGL_RESULT check_write_end(kgl_output_stream* st, KREQUEST r, KGL_RESULT result)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	if (result == KGL_OK && rq->ctx->know_length && rq->ctx->left_read != 0) {
		//有content-length，又未读完
		result = KGL_ESOCKET_BROKEN;
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
	check_write_end,
	check_release
};
static kgl_output_stream check_output_stream = {
	&check_stream_function
};
kgl_output_stream* get_check_output_stream()
{
	return &check_output_stream;
}
