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
#include "KHttpServer.h"
#include "KHttp2Upstream.h"
#ifdef ENABLE_BIG_OBJECT
#include "KBigObjectContext.h"
#endif
#include "KHttpFieldValue.h"
#ifndef NDEBUG
void debug_print_wsa_buf(WSABUF* buf, int bc) {
	for (int i = 0; i < bc; i++) {
		printf("\n*****chunk len=[%d]\n", (int)buf[i].iov_len);
		fwrite("[", 1, 1, stdout);
		fwrite(buf[i].iov_base, 1, buf[i].iov_len, stdout);
		fwrite("]\n", 1, 2, stdout);
	}
	fflush(stdout);
}
#endif
int process_post_fiber(void* arg, int got) {
	void** args = (void**)arg;
	KHttpRequest* rq = (KHttpRequest*)args[0];
	KAsyncFetchObject* fo = (KAsyncFetchObject*)args[1];
	delete[]args;
	int result = (int)fo->ProcessPost(rq);
	if (fo->client) {
		fo->client->shutdown();
	}
	return result;
}
#ifdef ENABLE_PROXY_PROTOCOL
KUpstream* proxy_tcp_connect(KHttpRequest* rq) {
	char ips[MAXIPLEN];
	auto proxy = rq->sink->get_proxy_info();
	if (proxy == NULL || proxy->dst == NULL) {
		return NULL;
	}
	if (!ksocket_sockaddr_ip(proxy->dst, ips, MAXIPLEN - 1)) {
		return NULL;
	}
	uint16_t port = ntohs(proxy->dst->v4.sin_port);
	KSingleAcserver* server = new KSingleAcserver("");
	server->set_proto(Proto_tcp);
	server->sockHelper->setHostPort(ips, port, NULL);
	server->sockHelper->setLifeTime(0);
	KUpstream* us = server->GetUpstream(rq);
	server->release();
	return us;
}
#endif
KUpstream* proxy_connect(KHttpRequest* rq) {
#ifdef ENABLE_PROXY_PROTOCOL
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_PROXY | WORK_MODEL_SSL_PROXY)) {
		return proxy_tcp_connect(rq);
	}
#endif
	const char* ip = NULL;
	KUrl* url = (KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_RAW_URL) ? rq->sink->data.raw_url : rq->sink->data.url);
	const char* host = url->host;
	u_short port = url->port;
	const char* ssl = NULL;
	int life_time = 2;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TPROXY) && KBIT_TEST(rq->ctx.filter_flags, RF_TPROXY_TRUST_DNS)) {
		if (KBIT_TEST(rq->ctx.filter_flags, RF_TPROXY_UPSTREAM)) {
			if (ip == NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->sink->get_connection()->st.fd, (struct sockaddr*)&s_sockaddr, &addr_len);
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
	if (rq->ctx.simulate) {
		KSimulateSink* ss = static_cast<KSimulateSink*>(rq->sink);
		if (ss->host) {
			host = ss->host.c_str();
			if (ss->port > 0) {
				port = ss->port;
			}
			life_time = ss->life_time;
		}
	}
#endif
	KRedirect* sa = server_container->refsRedirect(ip, host, port, ssl, life_time, Proto_http);
	if (sa == NULL) {
		return NULL;
	}
	KUpstream* us = sa->GetUpstream(rq);
	sa->release();
	return us;
}
void KAsyncFetchObject::on_readhup(KHttpRequest* rq) {
	if (client) {
		client->shutdown();
	}
	return;
}
void KAsyncFetchObject::ResetBuffer() {
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
KGL_RESULT KAsyncFetchObject::InternalProcess(KHttpRequest* rq, kfiber** post_fiber) {
	rq->ctx.left_read = -1;
	rq->ctx.upstream_connection_keep_alive = 1;
	rq->ctx.upstream_expected_done = 0;
	rq->readhup();
	if (brd == NULL) {
		client = proxy_connect(rq);
	} else {
		if (brd->rd->is_disable()) {
			return out->f->error(out->ctx,  STATUS_SERVICE_UNAVAILABLE, _KS("extend is disable"));
		}
		assert(client == NULL);
#ifdef HTTP_PROXY
		//proxy has upstream will build full url
		KBIT_SET(rq->ctx.filter_flags, RF_PROXY_FULL_URL);
#endif
		client = brd->rd->GetUpstream(rq);
	}
	if (client == NULL) {
		return upstream_is_error(rq, STATUS_GATEWAY_TIMEOUT, "Cann't connect to remote host");
	}
	client->BindOpaque(this);
	client->set_time_out(rq->sink->get_time_out());
	assert(rq->sink->get_selector() == kgl_get_tls_selector());
	int64_t post_length = in->f->body.get_left(in->body_ctx);	
	if (post_length == -1 && !client->IsMultiStream() && !KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		pop_header.post_is_chunk = 1;
	}
	KGL_RESULT ret = SendHeader(rq);
	if (ret != KGL_OK) {
		return ret;
	}
	if (post_length != 0 && !KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE | RQ_HAVE_EXPECT)) {
		ret = ProcessPost(rq);
		if (ret != KGL_END) {
			return ret;
		}
	}
	ret = ReadHeader(rq, post_fiber);
	if (ret == KGL_NO_BODY) {
		//fastcgi must meet END_REQUEST
		return read_body_end(rq, ret);
	}
	if (ret != KGL_OK) {
		return upstream_is_error(rq, STATUS_GATEWAY_TIMEOUT, "cann't read protocol header");
	}
	return ReadBody(rq);
}
KGL_RESULT KAsyncFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	body = { 0 };
	kfiber* post_fiber = NULL;
	this->in = in;
	this->out = out;
	KGL_RESULT result = InternalProcess(rq, &post_fiber);
	if (result == KGL_ECAN_RETRY_SOCKET_BROKEN && !client->IsNew() && !rq->ctx.read_huped) {
		/** only pooled upstream and can_retry_socket_broken error and not read_huped happen
		* then retry use a new upstream(not pool upstream).
		*/
		KBIT_SET(rq->sink->data.flags, RQ_UPSTREAM_ERROR);
		assert(post_fiber == NULL);
		Close(rq);
		result = InternalProcess(rq, &post_fiber);
	}
	if (result == KGL_ECAN_RETRY_SOCKET_BROKEN) {
		result = upstream_is_error(rq, STATUS_GATEWAY_TIMEOUT, "cann't send protocol header");
	} else if (result > 0 && client) {
		client->health(HealthStatus::Success);
	}
	if (post_fiber == NULL) {
		//提前关闭上流连接，及时释放资源
		Close(rq);
	}
	rq->ReleaseQueue();
	if (result == KGL_NO_BODY) {
		assert(body.ctx == nullptr);
		goto clean;
	}
	if (body.ctx) {
		result = body.f->close(body.ctx, result);
	}
clean:
	if (post_fiber != NULL) {
		WaitPostFiber(rq, post_fiber);
		Close(rq);
	}
	return result;
}

KGL_RESULT KAsyncFetchObject::ParseBody(KHttpRequest* rq) {
	char* data = us_buffer.buf;
	int len = us_buffer.used;
	KGL_RESULT ret = ParseBody(rq, &data, data + us_buffer.used);
	assert(us_buffer.buf);
	ks_save_point(&us_buffer, data);
	return ret;
}

KGL_RESULT KAsyncFetchObject::ReadBody(KHttpRequest* rq) {
	KGL_RESULT result;
	int len;
	for (;;) {
		if (us_buffer.used > 0) {
			result = ParseBody(rq);
			if (result != KGL_OK) {
				return result;
			}
		}
		if (!KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && !checkContinueReadBody(rq)) {
			return KGL_OK;
		}
		rq->readhup();
		if (KBIT_TEST(rq->ctx.filter_flags, RF_NO_BUFFER) && body.ctx) {
			result = body.f->flush(body.ctx);
			if (result!=KGL_OK) {
				return result;
			}
		}
		char* buf = (char*)getUpstreamBuffer(&len);
		int got = client->read(buf, len);
		if (got == 0) {
			KHttpHeader* header = client->get_trailer();
			while (header) {
				kgl_str_t name;
				kgl_get_header_name(header, &name);
				result = out->f->write_trailer(out->ctx, name.data, (hlen_t)name.len, header->buf + header->val_offset, (hlen_t)header->val_len);
				if (result != KGL_OK) {
					return result;
				}
				header = header->next;
			}
			return KGL_OK;
		}
		if (got < 0) {
			pop_header.keep_alive_time_out = -1;
			return  KGL_ESOCKET_BROKEN;
		}
		ks_write_success(&us_buffer, got);
	}
	return KGL_EUNKNOW;
}
KGL_RESULT KAsyncFetchObject::ReadHeader(KHttpRequest* rq, kfiber** post_fiber) {
	InitUpstreamBuffer();
#if defined(WORK_MODEL_TCP) || defined(HTTP_PROXY)
	//tcp pipe line
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && !client->IsMultiStream()) {
		return on_read_head_success(rq, post_fiber);
	}
#endif
	KGL_RESULT result = client->read_header();
	if (result != KGL_ENOT_SUPPORT) {
		if (result == KGL_OK) {
			return on_read_head_success(rq, post_fiber);
		}
		return result;
	}
	client->set_no_delay(false);
	int len;
	for (;;) {
		char* buf = (char*)getUpstreamBuffer(&len);
		int got = client->read(buf, len);
		if (got <= 0) {
			return KGL_ESOCKET_BROKEN;
		}
		ks_write_success(&us_buffer, got);
		char* data = us_buffer.buf;
		char* end = data + us_buffer.used;
		switch (parse_unknow_header(rq, &data, end)) {
		case kgl_parse_finished:
			ks_save_point(&us_buffer, data);
			return on_read_head_success(rq, post_fiber);
		case kgl_parse_continue:
			if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
				return KGL_EDATA_FORMAT;
			}
			ks_save_point(&us_buffer, data);
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
KGL_RESULT KAsyncFetchObject::ProcessPost(KHttpRequest* rq) {
	while (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) || in->f->body.get_left(in->body_ctx) != 0) {
		int len;
		char* buf = GetPostBuffer(&len);
		int got = in->f->body.read(in->body_ctx, buf, len);
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
KGL_RESULT KAsyncFetchObject::SendHeader(KHttpRequest* rq) {
	if (buffer == NULL) {
		KGL_RESULT result = buildHead(rq);
		if (result != KGL_OK) {
			return result;
		}
		kassert(buffer);
	}
	if (buffer->startRead() == 0) {
		return KGL_OK;
	}
	//debug_print_buff(buffer->getHead());
	client->set_delay();
	/*
	WSABUF buf[64];
	for (;;) {
		int bc = buffer->getReadBuffer(buf, kgl_countof(buf));
		int got = client->write(buf, bc);
		if (got <= 0) {
			return KGL_ECAN_RETRY_SOCKET_BROKEN;
		}
		if (!buffer->readSuccess(got)) {
			return KGL_OK;
		}
	}
	*/
	int length = buffer->getLen();
	int result = client->write_all(buffer->getHead(), length);
	if (result != 0) {
		return KGL_ECAN_RETRY_SOCKET_BROKEN;
	}
	buffer->readSuccess(length);
	assert(buffer->getLen() == 0);
	return KGL_OK;
}

KGL_RESULT KAsyncFetchObject::PostResult(KHttpRequest* rq, int got) {
	assert(buffer != NULL);
	//printf("handleReadPost got=[%d] protocol=[%d]\n",got,(int)rq->sink->data.http_major);
	if (got == 0 && !KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		KHttpHeader* trailer = rq->sink->get_trailer();
		if (pop_header.post_is_chunk) {
			buffer->WSTR("0\r\n");
		}
		while (trailer) {
			kgl_str_t name;
			kgl_get_header_name(trailer, &name);
			if (pop_header.post_is_chunk) {
				buffer->write_all(name.data, (int)name.len);
				buffer->WSTR(": ");
				buffer->write_all(trailer->buf + trailer->val_offset, trailer->val_len);
				buffer->WSTR("\r\n");
			} else {
				client->send_trailer(name.data, (hlen_t)name.len, trailer->buf + trailer->val_offset, trailer->val_len);
			}
			trailer = trailer->next;
		}
		//如果还有数据要读，而读到0的话，就表示读取失败，而不是读取结束。
		if (pop_header.post_is_chunk) {
			buffer->WSTR("\r\n");
			return KGL_OK;
		}
		client->write_end();
		return KGL_END;
	}
	if (got <= 0) {
		return KGL_ESOCKET_BROKEN;
		//return stageEndRequest(rq);
	}
	buffer->writeSuccess(got);
	if (pop_header.post_is_chunk) {
		BuildChunkHeader();
	}
	return KGL_OK;
}
void KAsyncFetchObject::BuildChunkHeader() {
	int len = buffer->getLen();
	//printf("~~~~~~~~~~~build chunk header len=[%d]\n", len);
	if (len == 0) {
		buffer->WSTR("0\r\n\r\n");
		return;
	}
	char buf[32];
	int buf_len = snprintf(buf, sizeof(buf), "%x\r\n", len);
	buffer->insert(buf, buf_len);
	buffer->WSTR("\r\n");
}
KGL_RESULT KAsyncFetchObject::SendPost(KHttpRequest* rq) {
	buildPost(rq);
	buffer->startRead();
	if (!KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		rq->readhup();
	}
	int length = buffer->getLen();
	int result = client->write_all(buffer->getHead(), length);
	if (result != 0) {
		return KGL_ECAN_RETRY_SOCKET_BROKEN;
	}
	buffer->readSuccess(length);
	assert(buffer->getLen() == 0);
	buffer->destroy();
	return KGL_OK;
}
void KAsyncFetchObject::create_post_fiber(KHttpRequest* rq, kfiber** post_fiber) {
	if (*post_fiber == NULL) {
		void** args = new void* [2];
		args[0] = rq;
		args[1] = this;
		kfiber_create(process_post_fiber, args, 2, http_config.fiber_stack_size, post_fiber);
	}
}
KGL_RESULT KAsyncFetchObject::on_read_head_success(KHttpRequest* rq, kfiber** post_fiber) {
#ifdef HTTP_PROXY
	if (rq->sink->data.meth == METH_CONNECT) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_UPGRADE);
	}
#endif
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		rq->sink->set_no_delay(true);
		client->set_no_delay(true);
		int tmo = rq->sink->get_time_out();
		if (tmo < 5) {
			tmo = 5;
			rq->sink->set_time_out(tmo);
		}
		client->set_time_out(tmo);
		create_post_fiber(rq, post_fiber);
	}
	if (pop_header.is_100_continue && KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT)) {
		assert(KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT));
		pop_header.is_100_continue = 0;
		if (*post_fiber == nullptr) {
			KGL_RESULT ret = ProcessPost(rq);
			if (ret != KGL_END) {
				return ret;
			}
			return ReadHeader(rq, post_fiber);
		}
	}
	return PushHeaderFinished(rq);
}
KGL_RESULT KAsyncFetchObject::PushHeaderFinished(KHttpRequest* rq) {
	if (rq->sink->data.meth == METH_HEAD || pop_header.no_body) {
		//expected done
		expectDone(rq);
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//如果是websocket，则长度未知
		rq->ctx.left_read = -1;
	}
	assert(body.ctx == nullptr);
	auto result =  out->f->write_header_finish(out->ctx, rq->ctx.left_read, &body);
	if (result != KGL_OK) {
		return result;
	}
	if (pop_header.upstream_is_chunk) {
		new_dechunk_body(out, &body);
	}
	return KGL_OK;
}
void KAsyncFetchObject::PushStatus(KHttpRequest* rq, int status_code) {
	if (status_code == 100) {
		pop_header.is_100_continue = 1;
		return;
	}
	pop_header.status_send = 1;
	if (is_status_code_no_body(status_code)) {
		pop_header.no_body = 1;
	}
	out->f->write_status(out->ctx,  status_code);
}
KGL_RESULT KAsyncFetchObject::PushHeader(KHttpRequest* rq, const char* attr, int attr_len, const char* val, int val_len, bool request_line) {
	/*
	if (attr) {
		printf("<<%.*s%s%.*s\n",attr_len, attr, (request_line ?" ":": "),val_len, val);
	}
	//*/
	if (!attr) {
		switch ((kgl_header_type)attr_len) {
		case kgl_header_status:
		{
			int status_code;
			if (kgl_parse_value_int(val, val_len, &status_code) == 0) {
				PushStatus(rq, status_code);
			}
			return KGL_OK;
		}
		default:
			return out->f->write_header(out->ctx, (kgl_header_type)attr_len, val, val_len);
		}
	}
	if (request_line && pop_header.proto == Proto_http) {
		if (strncasecmp(attr, "HTTP/", 5) == 0) {
			const char* dot = strchr(attr + 5, '.');
			if (dot == NULL) {
				return KGL_EDATA_FORMAT;
			}
			uint8_t http_major = *(dot - 1) - 0x30;//major;
			uint8_t http_minor = *(dot + 1) - 0x30;//minor;
			if (http_major > 1) {
				rq->ctx.upstream_connection_keep_alive = true;
			} else if (http_major == 1 && http_minor == 1) {
				rq->ctx.upstream_connection_keep_alive = true;
			}
			int status_code = kgl_atoi((u_char*)val, val_len);
			PushStatus(rq, status_code);
			return KGL_OK;
		}
	}
	if (pop_header.proto == Proto_spdy && *attr == ':') {
		attr++;
		attr_len--;
		if (strcasecmp(attr, "version") == 0) {
			return KGL_OK;
		}
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
	auto header = kgl_parse_response_header(attr, attr_len);
	switch (header) {
	case kgl_header_alt_svc:
		return KGL_OK;
	case kgl_header_transfer_encoding:
	{
		if (kgl_mem_case_same(val, val_len, _KS("chunked"))) {
			pop_header.upstream_is_chunk = 1;
			return KGL_OK;
		}
		break;
	}
	case kgl_header_keep_alive:
	{
		const char* end = val + val_len;
		const char* data = kgl_memstr(val, val_len, _KS("timeout="));
		if (data) {
			data += 8;
			//确保有效，减掉2秒生存时间
			pop_header.keep_alive_time_out = kgl_atoi((u_char*)data, end - data) - 2;
		}
		return KGL_OK;
	}
	case kgl_header_content_length:
	{
		int64_t content_length = kgl_atol((u_char*)val, val_len);
		if (rq->sink->data.meth == METH_HEAD) {
			rq->ctx.left_read = 0;
		} else {
			rq->ctx.left_read = content_length;
		}
		return KGL_OK;
		//return out->f->write_header(out->ctx,  header, (char*)&content_length, KGL_HEADER_VALUE_INT64);
	}
	case kgl_header_unknow:
		return out->f->write_unknow_header(out->ctx,  attr, attr_len, val, (hlen_t)val_len);
	default:
		break;
	}
	return out->f->write_header(out->ctx,  header, val, val_len);
}

kgl_parse_result KAsyncFetchObject::parse_unknow_header(KHttpRequest* rq, char** data, char* end) {
	khttp_parse_result rs;
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, data, end, &rs);
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
KGL_RESULT KAsyncFetchObject::ParseBody(KHttpRequest* rq, char** data, char* end) {
	size_t len = end - *data;
	if (len == 0) {
		return KGL_OK;
	}
#if 0
	printf("ParseBody len=[%d]\n", len);
	fwrite(*data, 1, len, stdout);
	printf("\n");
#endif
	assert(body.ctx);
	KGL_RESULT result = PushBody(rq, &body, *data, (int)len);
	(*data) += len;
	return result;
}
KGL_RESULT KAsyncFetchObject::upstream_is_error(KHttpRequest* rq, int error, const char* msg) {
	int err = errno;
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//return shutdown(rq);
	}
	if (!rq->ctx.read_huped) {
		KBIT_SET(rq->sink->data.flags, RQ_UPSTREAM_ERROR);
		if (client) {
			if (client->IsNew()) {
				//new的才计算错误.
				client->health(HealthStatus::Err);
			}
			auto url = rq->sink->data.url->getUrl();
			sockaddr_i* upstream_addr = client->GetAddr();
			char ips[MAXIPLEN];
			ksocket_sockaddr_ip(upstream_addr, ips, MAXIPLEN);
			klog(KLOG_WARNING, "rq=[%p] request=[%s %s] upstream=[%s:%d] self_port=[%d] error code=[%d],msg=[%s] errno=[%d %s],socket is %s.\n",
				rq,
				rq->get_method(),
				url.get(),
				ips,
				ksocket_addr_port(upstream_addr),
				client->GetSelfPort(),
				error,
				msg,
				err,
				strerror(err),
				(client->IsNew() ? "new" : "pool")
			);
		}
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return KGL_ESOCKET_BROKEN;
	}
	return out->f->error(out->ctx, error, msg, strlen(msg));
}
