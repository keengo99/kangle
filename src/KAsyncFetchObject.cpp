#include <errno.h>
#include "KAsyncFetchObject.h"
#include "http.h"
#include "kselector.h"
#include "KAsyncWorker.h"
#include "KSingleAcserver.h"
#include "KCdnContainer.h"
#include "KSimulateRequest.h"
#include "KHttpProxyFetchObject.h"
#include "KHttp2.h"
//{{ent
#include "KHttp2Upstream.h"
#ifdef ENABLE_BIG_OBJECT
#include "KBigObjectContext.h"
#endif//}}
#include "KHttpFieldValue.h"
#ifndef NDEBUG
void debug_print_wsa_buf(WSABUF* buf, int bc)
{
	for (int i = 0; i < bc; i++) {
		printf("\n*****chunk len=[%d]\n", (int)buf[i].iov_len);
		fwrite("[", 1, 1, stdout);
		fwrite(buf[i].iov_base, 1, buf[i].iov_len, stdout);
		fwrite("]\n", 1, 2, stdout);
	}
	fflush(stdout);
}
#endif
int process_post_fiber(void *arg, int got)
{
	void** args = (void**)arg;
	KHttpRequest* rq = (KHttpRequest*)args[0];
	KAsyncFetchObject* fo = (KAsyncFetchObject*)args[1];
	delete[]args;
	int result = (int)fo->ProcessPost(rq);
	if (fo->client) {
		fo->client->Shutdown();
	}
	return result;
}
#ifdef ENABLE_PROXY_PROTOCOL
KUpstream * proxy_tcp_connect(KHttpRequest *rq)
{
	char ips[MAXIPLEN];
	kconnection *cn = rq->sink->GetConnection();
	if (cn->proxy == NULL || cn->proxy->dst == NULL) {
		return NULL;
	}
	if (!ksocket_sockaddr_ip(cn->proxy->dst, ips, MAXIPLEN-1)) {
		return NULL;
	}
	uint16_t port = ntohs(cn->proxy->dst->v4.sin_port);
	KSingleAcserver *server = new KSingleAcserver;
	server->set_proto(Proto_tcp);
	server->sockHelper->setHostPort(ips, port, NULL);
	server->sockHelper->setLifeTime(0);
	KUpstream *us = server->GetUpstream(rq);
	server->release();
	return us;
}
#endif
KUpstream * proxy_connect(KHttpRequest *rq)
{
#ifdef ENABLE_PROXY_PROTOCOL
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_PROXY|WORK_MODEL_SSL_PROXY)) {
		return proxy_tcp_connect(rq);
	}
#endif
	//KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	const char *ip = rq->bind_ip;
	KUrl *url = (KBIT_TEST(rq->filter_flags,RF_PROXY_RAW_URL)?&rq->sink->data.raw_url:rq->sink->data.url);
	const char *host = url->host;
	u_short port = url->port;
	const char *ssl = NULL;
	int life_time = 2;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (KBIT_TEST(rq->GetWorkModel(),WORK_MODEL_TPROXY) && KBIT_TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
		if (KBIT_TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)) {
			if (ip==NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->sink->GetConnection()->st.fd, (struct sockaddr *) &s_sockaddr, &addr_len);
		ksocket_sockaddr_ip(&s_sockaddr, mip, MAXIPLEN);
		host = mip;
#ifdef KSOCKET_IPV6
		if (s_sockaddr.v4.sin_family == PF_INET6) {
			port = ntohs(s_sockaddr.v6.sin6_port);
		} else
#endif
		port = ntohs(s_sockaddr.v4.sin_port);
	}
#endif
#endif
	if (KBIT_TEST(url->flags, KGL_URL_ORIG_SSL)) {
		ssl = "s";
	}
#ifdef ENABLE_SIMULATE_HTTP
	/* simuate request must replace host and port */
	if (rq->ctx->simulate) {
		KSimulateSink *ss = static_cast<KSimulateSink *>(rq->sink);
		if (ss->host && *ss->host) {
			host = ss->host;
			if (ss->port>0) {
				port = ss->port;
			}
			life_time = ss->life_time;
		}
	}
#endif
	KRedirect *sa = server_container->refsRedirect(ip,host,port,ssl,life_time,Proto_http);
	if (sa==NULL) {
		return NULL;
	}
	KUpstream* us = sa->GetUpstream(rq);
	sa->release();
	return us;	
}

void KAsyncFetchObject::ResetBuffer()
{
	if (buffer) {
		delete buffer;
		buffer = NULL;
	}
	if (us_buffer.buf) {
		xfree(us_buffer.buf);
	}
	memset(&us_buffer, 0, sizeof(us_buffer));
	memset(&parser, 0, sizeof(parser));
	memset(&pop_header, 0, sizeof(pop_header));
}


KGL_RESULT KAsyncFetchObject::InternalProcess(KHttpRequest *rq, kfiber** post_fiber)
{
	rq->ctx->know_length = 0;
	rq->ctx->left_read = -1;
	pop_header.keep_alive_time_out = 0;
	rq->ctx->upstream_connection_keep_alive = 1;
	rq->ctx->upstream_expected_done = 0;
	if (brd == NULL) {
		client = proxy_connect(rq);
	} else {
		if (!brd->rd->enable) {
			return out->f->write_message(out, rq, KGL_MSG_ERROR, "extend is disable", STATUS_SERVICE_UNAVAILABLE);
		}
		badStage = BadStage_Connect;
		assert(client == NULL);
		client = brd->rd->GetUpstream(rq);
	}
	if (client == NULL) {
		if (!rq->ctx->read_huped) {
			KBIT_SET(rq->sink->data.flags, RQ_UPSTREAM_ERROR);
		}
		return out->f->write_message(out, rq, KGL_MSG_ERROR, "Cann't connect to remote host", STATUS_GATEWAY_TIMEOUT);
	}
	client->BindOpaque(this);
	client->SetTimeOut(rq->sink->GetTimeOut());
	client->BindSelector(rq->sink->GetSelector());
	int64_t post_length = in->f->get_read_left(in, rq);
	if (post_length == -1 && !client->IsMultiStream()) {
		chunk_post = 1;
	}

	KGL_RESULT ret = SendHeader(rq);
	if (ret != KGL_OK) {
		return ret;
	}
	if (post_length != 0) {
		ret = ProcessPost(rq);
		if (ret != KGL_END) {
			return ret;
		}
	}
	ret = ReadHeader(rq, post_fiber);	
	if (ret != KGL_OK) {
		return ret;
	}	
	return ReadBody(rq);
}
KGL_RESULT KAsyncFetchObject::Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out)
{
	kfiber* post_fiber = NULL;
	this->in = in;
	this->out = out;
	KGL_RESULT result = InternalProcess(rq, &post_fiber);
	if (result == KGL_RETRY) {
		assert(post_fiber == NULL);
		Close(rq);
		result = InternalProcess(rq, &post_fiber);
	}
	if (post_fiber == NULL) {
		//提前关闭上流连接，及时释放资源
		Close(rq);
	}
	rq->ReleaseQueue();
	if (result == KGL_NO_BODY) {
		goto clean;
	}
	result = this->out->f->write_end(this->out, rq, result);
clean:
	if (post_fiber != NULL) {
		WaitPostFiber(rq, post_fiber);
		Close(rq);
	}
	return result;
}

KGL_RESULT KAsyncFetchObject::ParseBody(KHttpRequest* rq)
{
	char* data = us_buffer.buf;
	int len = us_buffer.used;
	KGL_RESULT ret = ParseBody(rq, &data, &len);
	assert(us_buffer.buf);
	ks_save_point(&us_buffer, data, len);
	return ret;
}

KGL_RESULT KAsyncFetchObject::ReadBody(KHttpRequest* rq)
{
	int len;
	for (;;) {
		if (us_buffer.used > 0) {
			KGL_RESULT ret = ParseBody(rq);
			if (ret != KGL_OK) {
				return ret;
			}
		}
		if (!KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && !checkContinueReadBody(rq)) {
			return KGL_OK;
		}
		char *buf = (char*)getUpstreamBuffer(&len);
		WSABUF bufs;
		bufs.iov_base = buf;
		bufs.iov_len = len;
		int got = client->read(&bufs, 1);
		if (got <= 0) {
			pop_header.keep_alive_time_out = -1;
			return  got == 0 ? KGL_OK : KGL_ESOCKET_BROKEN;
		}
		ks_write_success(&us_buffer, got);
	}
	return KGL_EUNKNOW;
}
KGL_RESULT KAsyncFetchObject::ReadHeader(KHttpRequest *rq, kfiber **post_fiber)
{
	InitUpstreamBuffer();
#if defined(WORK_MODEL_TCP) || defined(HTTP_PROXY)
	//tcp pipe line
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		return readHeadSuccess(rq, post_fiber);
	}
#endif
	KGL_RESULT result = client->read_header();
	if (result!=KGL_ENOT_SUPPORT) {
		if (result == KGL_OK) {
			return readHeadSuccess(rq, post_fiber);
		}
		return result;
	}
	client->SetNoDelay(false);
	int len;
	for (;;) {		
		char *buf = (char *)getUpstreamBuffer(&len);
		WSABUF bufs;
		bufs.iov_base = buf;
		bufs.iov_len = len;
		int got = client->read(&bufs, 1);		
		if (got <= 0) {
			return KGL_ESOCKET_BROKEN;
		}
		ks_write_success(&us_buffer, got);
		char *data = us_buffer.buf;
		int len = us_buffer.used;
		switch (ParseHeader(rq, &data, &len)) {
		case kgl_parse_finished:
			ks_save_point(&us_buffer, data, len);
			return readHeadSuccess(rq, post_fiber);
		case kgl_parse_continue:
			if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
				return KGL_EDATA_FORMAT;
			}
			ks_save_point(&us_buffer, data, len);
			break;
		case kgl_parse_want_read:
			//ajp协议会发送一个JK_AJP13_GET_BODY_CHUNK过来，此时要发送一个空的数据包过去
			if (*post_fiber) {
				break;
			}
			if (SendPost(rq) != KGL_OK) {
				return KGL_ESOCKET_BROKEN;
			}
			break;
		default:
			return KGL_EDATA_FORMAT;
		}
	}
	return KGL_EUNKNOW;
}
KGL_RESULT KAsyncFetchObject::ProcessPost(KHttpRequest* rq)
{
	while (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) || in->f->get_read_left(in, rq) != 0) {
		int len;
		char* buf = GetPostBuffer(&len);
		int got = in->f->read_body(in, rq, buf, len);
		KGL_RESULT ret = PostResult(rq, got);
		if (ret != KGL_OK) {
			return ret;
		}
		ret = SendPost(rq);
		if (ret != KGL_OK) {
			return ret;
		}
	}
	return KGL_END;
}
KGL_RESULT KAsyncFetchObject::SendHeader(KHttpRequest* rq)
{
	if (buffer == NULL) {
		KGL_RESULT result = buildHead(rq);
		if (result!=KGL_OK) {
			return result;
		}
		kassert(buffer);
	}
	if (buffer->startRead() == 0) {
		return KGL_OK;
	}
	//debug_print_buff(buffer->getHead());
	client->SetDelay();
	WSABUF buf[16];
	for (;;) {
		int bc = buffer->getReadBuffer(buf, kgl_countof(buf));
		int got = client->write(buf, bc);
		if (got <= 0) {
			return KGL_ESOCKET_BROKEN;
		}
		if (!buffer->readSuccess(got)) {
			return KGL_OK;
		}
	}
	return KGL_EUNKNOW;
}

KGL_RESULT KAsyncFetchObject::PostResult(KHttpRequest *rq,int got)
{
	assert(buffer != NULL);
	//printf("handleReadPost got=[%d] protocol=[%d]\n",got,(int)rq->sink->data.http_major);
	if (got == 0 && !KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		if (chunk_post) {
			BuildChunkHeader();
			return KGL_OK;
		}
		if (in->f->get_read_left(in, rq) == 0) {
			//如果还有数据要读，而读到0的话，就表示读取失败，而不是读取结束。
			client->write_end();
			return KGL_END;
		}
	}
	if (got <= 0) {
		return KGL_ESOCKET_BROKEN;
		//return stageEndRequest(rq);
	}
	buffer->writeSuccess(got);
	if (chunk_post) {
		BuildChunkHeader();
	}
	return KGL_OK;
}
void KAsyncFetchObject::BuildChunkHeader()
{
	int len = buffer->getLen();
	//printf("~~~~~~~~~~~build chunk header len=[%d]\n", len);
	if (len == 0) {
		buffer->WSTRING("0\r\n\r\n");
		return;
	}
	char buf[32];
	int buf_len = snprintf(buf, sizeof(buf), "%x\r\n", len);
	buffer->insert(buf, buf_len);
	buffer->WSTRING("\r\n");
}
KGL_RESULT KAsyncFetchObject::SendPost(KHttpRequest *rq)
{
	buildPost(rq);
	buffer->startRead();
	KGL_RESULT result = KGL_OK;
	if (!KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//read_hup(rq);
	}
	WSABUF buf[16];
	for (;;) {
		int bc = buffer->getReadBuffer(buf, kgl_countof(buf));
		//debug_print_wsa_buf(buf, bc);
		int got = client->write(buf, bc);
		//printf("write upstream got=[%d]\n", got);
		if (got <= 0) {
			result = KGL_ESOCKET_BROKEN;
			break;
		}
		if (!buffer->readSuccess(got)) {
			break;
		}
	}
	buffer->destroy();
	return result;
}
void KAsyncFetchObject::CreatePostFiber(KHttpRequest* rq, kfiber** post_fiber)
{
	if (*post_fiber == NULL) {
		void** args = new void* [2];
		args[0] = rq;
		args[1] = this;
		kfiber_create(process_post_fiber, args, 2, conf.fiber_stack_size, post_fiber);
	}
}
KGL_RESULT KAsyncFetchObject::readHeadSuccess(KHttpRequest *rq,kfiber **post_fiber)
{
	pop_header.first_body_time = kgl_current_sec;
	client->IsGood();
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		rq->sink->SetNoDelay(true);
		client->SetNoDelay(true);
		int tmo = rq->sink->GetTimeOut();
		if (tmo < 5) {
			tmo = 5;
			rq->sink->SetTimeOut(tmo);
		}
		client->SetTimeOut(tmo);
		CreatePostFiber(rq, post_fiber);
	}
	return PushHeaderFinished(rq);
}
KGL_RESULT KAsyncFetchObject::PushHeaderFinished(KHttpRequest *rq)
{
	if (rq->sink->data.meth == METH_HEAD || pop_header.no_body) {
		//expected done
		expectDone(rq);
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//如果是websocket，则长度未知
		rq->ctx->know_length = 0;
		rq->ctx->left_read = -1;
	}
	return out->f->write_header_finish(out, rq);
}
void KAsyncFetchObject::PushStatus(KHttpRequest *rq, int status_code)
{
	pop_header.status_send = 1;
	if (is_status_code_no_body(status_code)) {
		pop_header.no_body = 1;
	}
	out->f->write_status(out, rq, status_code);
}
KGL_RESULT KAsyncFetchObject::PushHeader(KHttpRequest *rq, const char *attr, int attr_len, const char *val, int val_len, bool request_line)
{
	//printf("attr=[%s] val=[%s] request_line=[%d]\n", attr,val, request_line);
	if (request_line && pop_header.proto == Proto_http) {
		if (strncasecmp(attr, "HTTP/", 5) == 0) {
			const char *dot = strchr(attr + 5, '.');
			if (dot == NULL) {
				return KGL_EDATA_FORMAT;
			}
			uint8_t http_major = *(dot - 1) - 0x30;//major;
			uint8_t http_minor = *(dot + 1) - 0x30;//minor;
			if (http_major > 1) {
				rq->ctx->upstream_connection_keep_alive = true;
			} else if (http_major == 1 && http_minor == 1) {
				rq->ctx->upstream_connection_keep_alive = true;
			}
			PushStatus(rq, atoi(val));
			return KGL_OK;
		}
	}

	if (!strcasecmp(attr, "Keep-Alive")) {
		const char *data = strstr(val, "timeout=");
		if (data) {
			//确保有效，减掉2秒生存时间
			pop_header.keep_alive_time_out = atoi(data + 8) - 2;
		}
		return KGL_OK;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			out = new_dechunk_stream(out);
			rq->registerRequestCleanHook((kgl_cleanup_f)out->f->release, out);
			return KGL_OK;
		}
	}
	if (!strcasecmp(attr, "Connection")) {
		KHttpFieldValue field(val);
		do {
			if (field.is2("keep-alive", 10)) {
				rq->ctx->upstream_connection_keep_alive = true;
			} else if (field.is2("close", 5)) {
				rq->ctx->upstream_connection_keep_alive = false;
			} else if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) && field.is2("upgrade", 7)) {
				KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_UPGRADE);
				rq->ctx->upstream_connection_keep_alive = false;
			}
		} while (field.next());
		return KGL_OK;
	}

	if (pop_header.proto == Proto_spdy && *attr == ':') {
		attr++;
		attr_len--;
		if (strcasecmp(attr, "version") == 0) {
			return KGL_OK;
		}
	}
	if (!strcasecmp(attr, "Content-length")) {
		rq->ctx->know_length = 1;
		rq->ctx->left_read = string2int(val);
	}
	if (pop_header.proto != Proto_http) {
		if (!strcasecmp(attr, "Status")) {
			if (!pop_header.status_send) {
				PushStatus(rq, atoi(val));
			}
			return KGL_OK;
		}
		if (!strcasecmp(attr, "Location")) {
			if (!pop_header.status_send) {
				PushStatus(rq, STATUS_FOUND);
			}
		} else if (!strcasecmp(attr, "WWW-Authenticate")) {
			if (!pop_header.status_send) {
				PushStatus(rq, STATUS_UNAUTH);
			}
		}
	}
	return out->f->write_unknow_header(out, rq, attr, attr_len, val, val_len);
}

kgl_parse_result KAsyncFetchObject::ParseHeader(KHttpRequest *rq, char **data, int *len)
{
	khttp_parse_result rs;
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, data, len, &rs);
		switch (result) {
		case kgl_parse_continue:
			return kgl_parse_continue;
		case kgl_parse_success:
			if (KGL_OK != PushHeader(rq, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line)) {
				return kgl_parse_error;
			}
			//printf("attr=[%s] val=[%s]\n", rs.attr, rs.val);
			break;
		case kgl_parse_finished:
			return kgl_parse_finished;
		default:
			return kgl_parse_error;
		}
	}
}
KGL_RESULT KAsyncFetchObject::ParseBody(KHttpRequest *rq, char **data, int *len)
{
	KGL_RESULT result = PushBody(rq, out, *data, *len);
	*len = 0;
	return result;
}
KGL_RESULT KAsyncFetchObject::handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got)
{
	kassert(client);
#if 0
	int err = errno;
	pop_header.keep_alive_time_out = -1;
	if (!rq->ctx->read_huped) {
		if (client->IsNew()) {
			//new的才计算错误.
			client->IsBad(badStage);
		}
		KBIT_SET(rq->sink->data.flags, RQ_UPSTREAM_ERROR);
	}
	if (rq->ctx->connection_upgrade) {
		return shutdown(rq);
	}
	if (rq->ctx->read_huped) {
		//client broken
		return gate->f->push_message(gate, rq, KGL_MSG_ERROR, error, msg);
	}	
	char *url = rq->sink->data.url->getUrl();
	sockaddr_i *upstream_addr = client->GetAddr();
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(upstream_addr, ips, MAXIPLEN);
	klog(KLOG_WARNING, "rq=[%p] request=[%s %s] upstream=[%s:%d] self_port=[%d] error code=[%d],msg=[%s] errno=[%d %s],socket is %s,last_got=[%d].\n",
		rq,
		rq->getMethod(),
		url,
		ips,
		ksocket_addr_port(upstream_addr),
		client->GetSelfPort(),
		error,
		msg,
		err,
		strerror(err),
		(client->IsNew()?"new":"pool"),
		last_got
		);
	xfree(url);
	if (badStage != BadStage_SendSuccess && !client->IsNew()) {
		//use pool connectioin will try again
		assert(false);
		//return retryOpen(rq);
	}
	return gate->f->push_message(gate, rq, KGL_MSG_ERROR, error, msg);
#endif
	assert(false);
	return KGL_EUNKNOW;
}
