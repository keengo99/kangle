#include "KHttpSink.h"
#include "KHttpRequest.h"
#include "kbuf.h"
#include "http.h"
#include "kfiber.h"
#include "HttpFiber.h"

#define MAX_HTTP_CHUNK_SIZE 8192
kev_result delete_request_fiber(KOPAQUE data, void* arg, int got)
{
	KHttpRequest* rq = (KHttpRequest*)arg;
	delete rq;
	return kev_ok;
}
kev_result end_http_sink_fiber(KOPAQUE data, void* arg, int got)
{
	KHttpRequest* rq = (KHttpRequest*)arg;
	KHttpSink *sink = static_cast<KHttpSink*>(rq->sink);
	sink->EndFiber(rq);
	return kev_ok;
}
static int buffer_write_http_sink(KOPAQUE data, void *arg, LPWSABUF buf, int bc)
{
	KHttpSink *sink = (KHttpSink *)arg;
	kassert(sink->rc);
	return sink->rc->GetReadBuffer(data, buf, bc);
}
static void result_write_http_header(KOPAQUE data, void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KHttpSink *sink = static_cast<KHttpSink *>(rq->sink);
	kassert(sink->rc==NULL);
	if (got < 0) {
		SET(rq->flags, RQ_CONNECTION_CLOSE);
	}
	sink->EndRequest(rq);
}
static kev_result result_write_http_sink(KOPAQUE data, void *arg, int got)
{
	KHttpSink *sink = (KHttpSink *)arg;
	KResponseContext *rc = sink->rc;
	kassert(rc && rc->result);
	if (got <= 0) {
		return sink->ResultResponseContext(got);		
	}
	if (rc->ReadSuccess(&got)) {
		kassert(got == 0);
		return selectable_write(&sink->cn->st,result_write_http_sink, buffer_write_http_sink,arg);
	}
	return sink->ResultResponseContext(got);
}
kev_result result_skip_chunk_request(KOPAQUE data, void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got < 0) {
		delete rq;
		return kev_destroy;
	}
	KHttpSink *sink = static_cast<KHttpSink *>(rq->sink);
	if (got == 0) {
		rq->left_read = 0;
		sink->StartPipeLine(rq);
		return kev_ok;
	}
	sink->SkipPost(rq);
	return kev_ok;
}
kev_result result_skip_post(KOPAQUE data, void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got <= 0) {
		delete rq;
		return kev_destroy;
	}
	KHttpSink *sink = static_cast<KHttpSink *>(rq->sink);
	ks_write_success(&sink->buffer, got);
	sink->SkipPost(rq);
	return kev_ok;
}
kev_result result_read_http_sink(KOPAQUE data, void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got <= 0) {
		delete rq;
		return kev_destroy;
	}
	KHttpSink *sink = static_cast<KHttpSink *>(rq->sink);
	//fwrite(sink->buffer.buf+sink->buffer.used, 1, got, stdout);
	ks_write_success(&sink->buffer, got);	
	return sink->Parse(rq);
}

int buffer_read_http_sink(KOPAQUE data, void *arg, LPWSABUF buf, int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KHttpSink *sink = static_cast<KHttpSink *>(rq->sink);
	int bc = ks_get_write_buffers(&sink->buffer, buf, bufCount);
	//printf("buf_len=[%d]\n", buf[0].len);
	return bc;
}
#ifdef KSOCKET_SSL
int handle_http2https_error(void *arg, int got)
{
	KHttpRequest* rq = (KHttpRequest*)arg;
	SET(rq->flags, RQ_CONNECTION_CLOSE);
	if (conf.http2https_code == 0 || rq->raw_url.IsBad()) {
		send_error2(rq, STATUS_HTTP_TO_HTTPS, "send http to https port");
		goto clean;
	}
	sockaddr_i addr;
	if (!rq->sink->GetSelfAddr(&addr)) {
		send_error2(rq, STATUS_HTTP_TO_HTTPS, "send http to https port");
		goto clean;
	}
	{
		KStringBuf s;
		s << "https://";
		rq->raw_url.GetHost(s, 443);
		rq->raw_url.GetPath(s, false);
		push_redirect_header(rq, s.getBuf(), s.getSize(), conf.http2https_code);
		rq->startResponseBody(0);
	}
clean:
	rq->EndRequest();
	return 0;
}
#endif
kev_result KHttpSink::ResultResponseContext(int got)
{
	buffer_callback buffer = rc->buffer;
	result_callback result = rc->result;
	void *arg = rc->arg;
	kassert(got < 0 || rc->GetLen() == 0);
	delete rc;
	rc = NULL;
	if (got==0 && buffer) {
		return selectable_write(&cn->st,result, buffer, arg);
	}
	return result(cn->st.data, arg, got);
}
KHttpSink::KHttpSink(kconnection *c)
{
	this->cn = c;
	ks_buffer_init(&buffer, MAX_HTTP_CHUNK_SIZE);
	memset(&parser, 0, sizeof(parser));
	rc = NULL;
	dechunk = NULL;
}
KHttpSink::~KHttpSink() {
	//printf("~KHttpSink\n");
	if (dechunk) {
		delete dechunk;
	}
	if (buffer.buf) {
		xfree(buffer.buf);
	}
	kconnection_destroy(cn);
}
void KHttpSink::StartHeader(KHttpRequest *rq)
{
	if (rc == NULL) {
		rc = new KResponseContext(rq->pool);
	}
}
bool KHttpSink::ResponseStatus(KHttpRequest *rq, uint16_t status_code)
{
	kassert(rc);
	kgl_str_t request_line;
	getRequestLine(rq->pool, status_code, &request_line);
	rc->head_insert_const(request_line.data, (uint16_t)request_line.len);
	return true;
}
kev_result KHttpSink::Parse(KHttpRequest *rq)
{
	khttp_parse_result rs;
	char *hot = buffer.buf;
	int len = buffer.used;
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, &hot, &len, &rs);
		//printf("len=[%d],result=[%d]\n", len,result);
		//fwrite(hot, 1, len, stdout);
		switch (result) {
		case kgl_parse_continue:
		{
			if (kgl_current_msec - rq->begin_time_msec > conf.time_out * 2000) {
				delete rq;
				return kev_destroy;
			}			
			if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
				delete rq;
				return kev_destroy;
			}
			ks_save_point(&buffer, hot, len);	
			return ReadHeader(rq);
		}
		case kgl_parse_success:
			if (!rq->ParseHeader(rs.attr, rs.attr_len, rs.val, rs.val_len, rs.is_first)) {
				delete rq;
				return kev_destroy;
			}
			break;
		case kgl_parse_finished:
			kassert(rc == NULL);
			ksocket_delay(cn->st.fd);
			ks_save_point(&buffer, hot, len);
			if (TEST(rq->flags, RQ_INPUT_CHUNKED)) {
				kassert(dechunk == NULL);
				dechunk = new KDechunkContext;
			}
			//printf("***************body_len=[%d]\n", parser.body_len);
			rc = new KResponseContext(rq->pool);
#ifdef KSOCKET_SSL	
			if (kconnection_is_ssl_not_handshake(cn)) {
				kfiber_create(handle_http2https_error, rq, 0, conf.fiber_stack_size, NULL);
				return kev_ok;
			}
#endif
			kfiber_create(start_request_fiber, rq, parser.header_len, conf.fiber_stack_size, NULL);
			return kev_ok;
		default:
			delete rq;
			return kev_destroy;
		}
	}
}
bool KHttpSink::ResponseHeader(const char *name, int name_len, const char *val, int val_len)
{
	kassert(rc);
	int len = name_len + val_len + 4;
	char *buf = (char *)kgl_pnalloc(rc->ab.pool,len);
	char *hot = buf;
	kgl_memcpy(hot, name, name_len);
	hot += name_len;
	kgl_memcpy(hot, ": ", 2);
	hot += 2;
	kgl_memcpy(hot, val, val_len);
	hot += val_len;
	kgl_memcpy(hot, "\r\n", 2);
	rc->head_append(buf, len);
	return true;
}
int KHttpSink::StartResponseBody(KHttpRequest *rq,int64_t body_size)
{
	if (rc == NULL) {
		return 0;
	}
	if (!rq->ctx->connection_upgrade) {
		ksocket_delay(cn->st.fd);
	}
	rc->head_append_const("\r\n", 2);
	rc->SwitchRead();
	int header_len = rc->GetLen();
#if 0
	if (rq->IsSync()) {
		//TODO: ¹ýÊ±
		WSABUF buf[64];
		for (;;) {
			int bc = rc->GetReadBuffer(cn->st.data, buf, 64);
			kassert(bc > 0);
			int got = selectable_sync_write(&cn->st, buf, bc);
			if (got <= 0) {
				header_len = -1;
				break;
			}			
			if (!rc->ReadSuccess(&got)) {
				kassert(got == 0);
				break;
			}
		}
		delete rc;
		rc = NULL;
	} else 
#endif
	if (!kfiber_is_main()) {
		WSABUF buf[64];
		for (;;) {
			int bc = rc->GetReadBuffer(cn->st.data, buf, 64);
			kassert(bc > 0);
			int got = kfiber_net_writev(cn, buf, bc);
			if (got <= 0) {
				header_len = -1;
				break;
			}
			if (!rc->ReadSuccess(&got)) {
				kassert(got == 0);
				break;
			}
		}
		delete rc;
		rc = NULL;
	}
	return header_len;
}
bool KHttpSink::IsLocked()
{
	return TEST(cn->st.st_flags,STF_LOCK);
}
int KHttpSink::Read(char *buf, int len)
{	
	if (dechunk) {
		return dechunk->Read(this, buf, len);
	}
	if (buffer.used > 0) {
		len = MIN(len, buffer.used );
		kgl_memcpy(buf, buffer.buf, len);
		ks_save_point(&buffer, buffer.buf + len, buffer.used - len);
		return len;
	}
	return kfiber_net_read(cn, buf, len);
#if 0
	WSABUF bufs;
	bufs.iov_len = len;
	bufs.iov_base = buf;
	//return selectable_sync_read(&cn->st, &bufs, 1);
#endif
}
int KHttpSink::Write(LPWSABUF buf, int bc)
{
	assert(!kfiber_is_main());
	return kfiber_net_writev(cn, buf, bc);
}
#if 0
kev_result KHttpSink::Write(void *arg,result_callback result, buffer_callback buffer)
{
	if (rc == NULL) {
		return selectable_write(&cn->st, result, buffer, arg);
	}
	rc->arg = arg;
	rc->result = result;
	rc->buffer = buffer;
	return selectable_write(&cn->st, result_write_http_sink, buffer_write_http_sink, this);
}
kev_result KHttpSink::Read(void *arg,result_callback result, buffer_callback buffer)
{
	if (dechunk) {
		return dechunk->Read(this,arg,result,buffer);
	}
	if (this->buffer.used > 0) {
		WSABUF buf;
		int bc = buffer(cn->st.data, arg, &buf, 1);
		kassert(bc == 1);
		int len = MIN((int)buf.iov_len, this->buffer.used);
		kgl_memcpy(buf.iov_base, this->buffer.buf, len);
		ks_save_point(&this->buffer, this->buffer.buf + len, this->buffer.used - len);
		return result(cn->st.data, arg, len);
	}
	return selectable_read(&cn->st, result, buffer, arg);
}
#endif
int KHttpSink::EndRequest(KHttpRequest *rq) 
{
	if (rc) {
		delete rc;
		rc = NULL;
		SET(rq->flags, RQ_CONNECTION_CLOSE);
	}
	if (TEST(rq->flags, RQ_CONNECTION_CLOSE) || !TEST(rq->flags, RQ_HAS_KEEP_CONNECTION)) {
		return kfiber_exit_callback(NULL, delete_request_fiber, rq);
	}
	ksocket_no_delay(cn->st.fd,false);
	kassert(buffer.buf_size > 0);
	if (rq->left_read != 0 && !TEST(rq->flags, RQ_HAVE_EXPECT)) {
		//still have data to read
		SkipPost(rq);
		//delete rq;
		return 0;
	}
	return StartPipeLine(rq);
}
void KHttpSink::SkipPost(KHttpRequest *rq)
{
	kassert(rq->left_read != 0);
	if (dechunk) {
		for (;;) {
			int got = dechunk->Read(this, NULL, 8192);
			if (got < 0) {
				kfiber_exit_callback(NULL, delete_request_fiber, rq);
				return;
			}
			KHttpSink* sink = static_cast<KHttpSink*>(rq->sink);
			if (got == 0) {
				rq->left_read = 0;
				sink->StartPipeLine(rq);
				return;
			}
			rq->AddUpFlow((INT64)got);
		}
		return;
	}	
	int buf_size;
	char *buf = ks_get_write_buffer(&buffer, &buf_size);
	int skip_len = (int)(MIN(rq->left_read, (INT64)buffer.used));
	ks_save_point(&buffer, buffer.buf + skip_len, buffer.used - skip_len);
	rq->left_read -= skip_len;
	rq->AddUpFlow((INT64)skip_len);
	if (rq->left_read <= 0) {
		StartPipeLine(rq);
		return;
	}
	while (rq->left_read > 0) {
		int len = kfiber_net_read(cn, buf, (int)MIN((int64_t)buf_size, rq->left_read));
		if (len <= 0) {
			kfiber_exit_callback(NULL, delete_request_fiber, rq);
			return;
		}
		rq->left_read -= len;
	}
	StartPipeLine(rq);
}
void KHttpSink::EndFiber(KHttpRequest* rq)
{
	if (buffer.used > 0) {
		Parse(rq);
		return;
	}
	ReadHeader(rq);
}
int KHttpSink::StartPipeLine(KHttpRequest *rq)
{
	kassert(rq->left_read == 0 || TEST(rq->flags, RQ_HAVE_EXPECT));	
	rq->clean();
	rq->init(NULL);

	memset(&parser, 0, sizeof(parser));
	if (dechunk) {
		delete dechunk;
		dechunk = NULL;
	}
	return kfiber_exit_callback(NULL, end_http_sink_fiber, rq);	
}
kev_result KHttpSink::ReadHeader(KHttpRequest *rq)
{
	return selectable_read(&cn->st, result_read_http_sink, buffer_read_http_sink, rq);
}
