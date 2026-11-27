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
#include "KHttpKeyValue.h"
#include "HttpFiber.h"
#ifdef ENABLE_BIG_OBJECT
#include "KBigObjectContext.h"
#endif
#include "KHttpFieldValue.h"
#include "KHttpValidation.h"
#include "KVirtualHost.h"
#include "KPushGate.h"
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
	fo->pop_header.post_finished = 1;
	/* gRPC and a half-closed tunnel still read the response after KGL_END */
	if (fo->client && result != KGL_END) {
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
	KUrl* url = (KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_RAW_URL) ? &rq->sink->data.raw_url : rq->sink->data.url);
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
#ifdef KSOCKET_SSL
static bool tls_eof_without_notify(kconnection* cn) {
	return cn && cn->st.ssl && cn->st.ssl->eof_without_notify;
}
#else
static bool tls_eof_without_notify(void*) {
	return false;
}
#endif
bool KAsyncFetchObject::tunnel_write_end() {
	if (client->IsMultiStream()) {
		/* HTTP/2 or HTTP/3 stream: END_STREAM */
		client->write_end();
		return true;
	}
	kconnection* cn = client->get_connection();
	if (cn == nullptr) {
		return false;
	}
#ifdef KSOCKET_SSL
	if (cn->st.ssl) {
#ifdef ENABLE_KSSL_BIO
		/* close_notify would wait in the memory BIO for a write */
		return false;
#else
		if (cn->st.base.selector != kgl_get_tls_selector()) {
			/* the SSL object belongs to another thread */
			return false;
		}
		/* Send close_notify once. The response reader keeps using SSL_read;
		 * marking the session shut down stops the close path from waiting
		 * for the origin's close_notify. Without an end the origin would
		 * wait for more data, and an idle WebSocket tunnel never times out. */
		ERR_clear_error();
		SSL_shutdown(cn->st.ssl->ssl);
		ERR_clear_error();
		cn->st.ssl->shutdown = 1;
#endif
	}
#endif
	ksocket_shutdown(cn->st.fd, SHUT_WR);
	return true;
}
void KAsyncFetchObject::ResetBuffer() {
	free_header_list(early_headers.steal_header());
	early_bytes = 0;
	early_overflow = false;
	forward_early_hints = false;
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
	if (client->IsMultiStream() && rq->sink->data.http_version == 0x101) {
		for (auto h = rq->sink->data.get_header(); h; h = h->next) {
			if (!kgl_is_attr(h, _KS("TE"))) continue;
			KHttpFieldValue field(h->buf + h->val_offset, h->buf + h->val_offset + h->val_len);
			do {
				if (field.is(_KS("trailers"))) rq->sink->enable_response_trailers();
			} while (field.next());
		}
	}
	assert(rq->sink->get_selector() == kgl_get_tls_selector());
	int64_t post_length = in->f->body.get_left(in->body_ctx);
	bool need_request_end = upstream_chunk_post() && post_length == 0 && rq->sink->need_request_end();
	if (need_request_end && client->IsMultiStream()) client->set_duplex(true);
	if (upstream_chunk_post() && !client->IsMultiStream() && !KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		// HTTP/1 origins need chunked framing for trailers. Known-length
		// requests otherwise retain Content-Length and stream without buffering;
		// clients wanting trailers must declare them before the body is sent.
		for (auto h=rq->sink->data.get_header(); h; h=h->next) {
			if (kgl_is_attr(h, _KS("Trailer"))) pop_header.post_is_chunk = 1;
		}
	}
	if (client->IsMultiStream() && upstream_chunk_post()) {
		for (auto h=rq->sink->data.get_header(); h; h=h->next) {
			if (!kgl_is_attr(h, _KS("Content-Type"))) continue;
			const char* type=h->buf+h->val_offset;
			if (h->val_len >= 16 && strncasecmp(type,"application/grpc",16)==0 &&
				(h->val_len==16 || type[16]=='+' || type[16]==';')) {
				pop_header.grpc=1;
				client->set_duplex(true);
				client->set_no_delay(true);
				rq->sink->set_no_delay(true);
				KBIT_SET(rq->ctx.filter_flags, RF_NO_BUFFER);
				KBIT_SET(rq->sink->data.flags, RQ_HAS_NO_CACHE);
			}
		}
	}
	if (post_length == -1 && upstream_chunk_post() && !client->IsMultiStream() && !KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		pop_header.post_is_chunk = 1;
	}
	KGL_RESULT ret = SendHeader(rq);
	if (ret != KGL_OK) {
		return ret;
	}
	if (pop_header.grpc) {
		create_post_fiber(rq, post_fiber);
	} else if ((post_length != 0 || need_request_end) && !KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) &&
		(!KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT) || !upstream_handle_expect())) {
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
	if (ret == KGL_ESOCKET_BROKEN && !client->IsNew() && kgl_is_safe_method(rq->sink->data.meth)) {
		// The peer may close an idle keep-alive connection while it is in the
		// pool.  If no response bytes were received, replaying a safe request on
		// a fresh connection is preferable to exposing that normal race as a 504.
		return KGL_ECAN_RETRY_SOCKET_BROKEN;
	}
	if (ret != KGL_OK) {
		// A connected origin can reject one client or request without returning
		// HTTP headers. Do not disable the shared node for a response failure;
		// it is reported as ResponseErr, which still recycles a hung local process.
		return upstream_is_error(rq, STATUS_GATEWAY_TIMEOUT, "cann't read protocol header", false);
	}
	return ReadBody(rq);
}
KGL_RESULT KAsyncFetchObject::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	body = { 0 };
	kfiber* post_fiber = NULL;
	this->in = in;
	this->out = out;
	KGL_RESULT result = InternalProcess(rq, &post_fiber);
	if (result == KGL_ECAN_RETRY_SOCKET_BROKEN && post_fiber == NULL && !client->IsNew() && !rq->ctx.read_huped &&
		(!KBIT_TEST(rq->sink->data.flags, RQ_HAS_READ_POST) || rq->ctx.request_body_replayable)) {
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
		if (got < 0 && KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && !client->IsMultiStream() &&
			tls_eof_without_notify(client->get_connection())) {
			/* Many TLS origins close a tunnel without close_notify. Treat it as
			 * the FIN it is, so the client stream ends instead of being reset. */
			got = 0;
		}
		if (got == 0) {
			expectDone(rq);
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
	bool received_data = false;
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
			return received_data ? KGL_EDATA_FORMAT : KGL_ESOCKET_BROKEN;
		}
		received_data = true;
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
		if (pop_header.post_ended) return KGL_END;
	}
	if (!rq->sink->finish_read_body()) return KGL_EDATA_FORMAT;
	KGL_RESULT ret = PostResult(rq, 0);
	if (ret != KGL_OK) return ret;
	ret = SendPost(rq);
	return ret == KGL_OK ? KGL_END : ret;
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
	// Cork only when request body data follows, so the header and body share
	// packets. A bodyless request is one write and needs no setsockopt calls.
	if (!pop_header.grpc && (in->f->body.get_left(in->body_ctx) != 0 || rq->sink->need_request_end())) {
		client->set_delay();
	}
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
		if (in->f->body.get_left(in->body_ctx) > 0) {
			rq->sink->shutdown();
			return KGL_EDATA_FORMAT;
		}
		pop_header.post_ended = 1;
		KHttpHeader* trailer = rq->sink->get_trailer();
		if (trailer && upstream_chunk_post() && !client->IsMultiStream() && !pop_header.post_is_chunk) {
			// Content-Length has already been sent. Do not silently discard the
			// trailers or append them as bytes outside the framed request body.
			client->shutdown();
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			KGL_RESULT result = send_error2(rq, STATUS_BAD_REQUEST, "request trailers require a Trailer header for Content-Length HTTP/1 upstream");
			return result == KGL_OK ? KGL_EHAS_SEND_HEADER : result;
		}
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
				if (!client->send_trailer(name.data, (hlen_t)name.len, trailer->buf + trailer->val_offset, trailer->val_len)) return KGL_EDATA_FORMAT;
			}
			trailer = trailer->next;
		}
		//如果还有数据要读，而读到0的话，就表示读取失败，而不是读取结束。
		if (pop_header.post_is_chunk) {
			buffer->WSTR("\r\n");
			return KGL_OK;
		}
		KGL_RESULT end_result = on_post_end();
		if (end_result != KGL_OK) {
			return end_result;
		}
		client->write_end();
		return KGL_END;
	}
	if (got == KGL_EDENIED) {
		// A request-body filter rejected the payload. The remainder of the
		// client body must not be interpreted as another keep-alive request.
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		KGL_RESULT result = send_error2(rq, STATUS_FORBIDEN, "denied by request body filter");
		return result == KGL_OK ? KGL_EHAS_SEND_HEADER : result;
	}
	if (got == 0 && KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE) && !pop_header.post_canceled &&
		tunnel_write_end()) {
		/* The client ended its side of the tunnel (HTTP/2 END_STREAM or a
		 * TCP close). The end was passed on; keep relaying what the origin
		 * still sends, such as its WebSocket close frame. */
		pop_header.post_ended = 1;
		return KGL_END;
	}
	if (got <= 0) {
		if (!pop_header.post_canceled) rq->sink->shutdown();
		return KGL_ESOCKET_BROKEN;
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
KGL_RESULT KAsyncFetchObject::StoreEarlyHint(const char* name, int name_len, const char* value, int value_len) {
	if (!forward_early_hints || early_overflow) return KGL_OK;
	// An oversized informational block can be ignored without losing the final response.
	early_bytes += (size_t)name_len + value_len + 4;
	if (early_bytes > 65536) {
		early_overflow = true;
		free_header_list(early_headers.steal_header());
		return KGL_OK;
	}
	return early_headers.add_header(name, name_len, value, value_len) ? KGL_OK : KGL_EDATA_FORMAT;
}
KGL_RESULT KAsyncFetchObject::ForwardEarlyHints(KHttpRequest* rq) {
	KHttpHeaderManager filtered{};
	if (forward_early_hints && !early_overflow && !rq->sink->data.status_code) {
		for (auto h = early_headers.header; h; h = h->next) {
			const char* name = h->buf;
			int len = h->name_len;
			bool skip = khttp_field_equal(name, len, "connection") || khttp_field_equal(name, len, "keep-alive") ||
				khttp_field_equal(name, len, "proxy-connection") || khttp_field_equal(name, len, "upgrade") ||
				khttp_field_equal(name, len, "transfer-encoding") || khttp_field_equal(name, len, "content-length") ||
				khttp_field_equal(name, len, "trailer") || khttp_field_equal(name, len, "te");
			// A HTTP/1 origin can nominate additional hop-by-hop fields.
			for (auto c = early_headers.header; !skip && c; c = c->next) {
				if (!kgl_is_attr(c, _KS("Connection"))) continue;
				const char* pos = c->buf + c->val_offset;
				const char* end = pos + c->val_len;
				while (pos < end) {
					const char* comma = (const char*)memchr(pos, ',', end - pos);
					if (!comma) comma = end;
					const char* token_end = comma;
					while (pos < token_end && (*pos == ' ' || *pos == '\t')) ++pos;
					while (token_end > pos && (token_end[-1] == ' ' || token_end[-1] == '\t')) --token_end;
					if (token_end - pos == len && strncasecmp(name, pos, len) == 0) skip = true;
					pos = comma == end ? end : comma + 1;
				}
			}
			if (!skip && !filtered.add_header(name, len, h->buf + h->val_offset, h->val_len)) {
				free_header_list(filtered.steal_header());
				return KGL_EDATA_FORMAT;
			}
		}
	}
	bool success = true;
	if (forward_early_hints && !early_overflow && !rq->sink->data.status_code) {
		success = rq->sink->response_early_hints(filtered.header);
	}
	free_header_list(filtered.steal_header());
	free_header_list(early_headers.steal_header());
	early_bytes = 0;
	early_overflow = false;
	forward_early_hints = false;
	if (!success) rq->ctx.read_huped = 1;
	return success ? KGL_OK : KGL_ESOCKET_BROKEN;
}
KGL_RESULT KAsyncFetchObject::on_read_head_success(KHttpRequest* rq, kfiber** post_fiber) {
	if (pop_header.is_interim_response && (pop_header.proto == Proto_http || pop_header.proto == Proto_spdy)) {
		// Only 100 permits sending an Expect body; 102/103 must keep waiting.
		if (++pop_header.interim_count > 8) {
			return KGL_EDATA_FORMAT;
		}
		KGL_RESULT hint_result = ForwardEarlyHints(rq);
		if (hint_result != KGL_OK) return hint_result;
		bool send_post = pop_header.is_100_continue &&
			KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT) && *post_fiber == nullptr;
		pop_header.is_interim_response = 0;
		pop_header.is_100_continue = 0;
		pop_header.field_count = 0;
		if (send_post) {
			KGL_RESULT ret = ProcessPost(rq);
			if (ret != KGL_END) {
				return ret;
			}
		}
		return ReadHeader(rq, post_fiber);
	}
#ifdef HTTP_PROXY
	if (rq->sink->data.meth == METH_CONNECT) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_UPGRADE);
	}
#endif
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		/* On Linux set_no_delay(false) removes the TCP_CORK taken while the
		 * request head was parsed. TCP_NODELAY alone does not override it, so
		 * every tunnel write would wait for the 200ms cork ceiling. */
		rq->sink->set_no_delay(false);
		rq->sink->set_no_delay(true);
		client->set_no_delay(false);
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
	if (pop_header.upstream_is_chunk && rq->sink->data.meth != METH_HEAD && !pop_header.no_body) {
		//Transfer-Encoding: chunked 优先于 Content-Length (RFC 7230 3.3.3)
		rq->ctx.left_read = -1;
	}
	assert(body.ctx == nullptr);
	// HEAD has no response body, but its Content-Length describes a GET.
	int64_t response_length = rq->ctx.left_read;
	if (rq->sink->data.meth == METH_HEAD && !pop_header.no_body) {
		response_length = pop_header.has_content_length ? pop_header.response_length : -1;
	}
	auto result = out->f->write_header_finish(out->ctx, response_length, &body);
	if (result != KGL_OK) {
		return result;
	}
	if (pop_header.upstream_is_chunk && body.ctx) {
		new_dechunk_body(out, &body);
	}
	return KGL_OK;
}
void KAsyncFetchObject::PushStatus(KHttpRequest* rq, int status_code) {
	if (status_code == 100 ||
		((pop_header.proto == Proto_http || pop_header.proto == Proto_spdy) && status_code > 100 && status_code < 200 && status_code != 101)) {
		pop_header.is_100_continue = status_code == 100;
		pop_header.is_interim_response = 1;
		unsigned mode = rq->ctx.early_hints;
		if (!rq->ctx.early_hints_set) {
			auto svh = kangle::get_virtual_host(rq);
			if (svh) mode = svh->vh->early_hints;
		}
		forward_early_hints = status_code == 103 && mode &&
			!rq->ctx.internal && !rq->ctx.simulate && !rq->ctx.no_http_header &&
			(rq->sink->data.http_version == 0x200 || (mode == 1 && rq->sink->data.http_version == 0x300) ||
			 (mode == 1 && rq->sink->data.http_version == 0x101));
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
			if (pop_header.is_interim_response && (pop_header.proto == Proto_http || pop_header.proto == Proto_spdy)) {
				if (attr_len < 0 || attr_len >= (int)kgl_header_unknow) return KGL_EDATA_FORMAT;
				const auto& name = kgl_header_type_string[attr_len].value;
				return PushHeader(rq, name.data, (int)name.len, val, val_len, false);
			}
			return out->f->write_header(out->ctx, (kgl_header_type)attr_len, val, val_len);
		}
	}
	if (request_line && pop_header.proto == Proto_http) {
		if (attr_len==8 && (memcmp(attr,"HTTP/1.0",8)==0 || memcmp(attr,"HTTP/1.1",8)==0) &&
			val_len>=3 && val[0]>='1' && val[0]<='5' && val[1]>='0' && val[1]<='9' && val[2]>='0' && val[2]<='9' &&
			(val_len==3 || val[3]==' ')) {
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
			} else {
				//HTTP/1.0默认不保持连接，除非后续有Connection: keep-alive
				rq->ctx.upstream_connection_keep_alive = false;
			}
			int status_code = kgl_atoi((u_char*)val, val_len);
			PushStatus(rq, status_code);
			return KGL_OK;
		}
		return KGL_EDATA_FORMAT;
	}
	if (++pop_header.field_count > KHTTP_MAX_FIELD_COUNT || !khttp_valid_field(attr, attr_len, val, val_len)) return KGL_EDATA_FORMAT;
	if (pop_header.is_interim_response && (pop_header.proto == Proto_http || pop_header.proto == Proto_spdy)) {
		return StoreEarlyHint(attr, attr_len, val, val_len);
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
		if (kgl_mem_case_same(val, val_len, _KS("chunked")) && !pop_header.upstream_is_chunk) {
			pop_header.upstream_is_chunk = 1;
			// Preserve compatibility with HTTP/1 origins sending both fields.
			// Discard Content-Length, decode chunked, and retire this connection.
			if (pop_header.has_content_length) pop_header.keep_alive_time_out = -1;
			return KGL_OK;
		}
		return KGL_EDATA_FORMAT;
	}
	case kgl_header_keep_alive:
	{
		const char* end = val + val_len;
		const char* data = kgl_memstr(val, val_len, _KS("timeout="));
		if (data) {
			data += 8;
			//确保有效，减掉2秒生存时间
			int keep_alive_time_out = kgl_atoi((u_char*)data, end - data);
			//0 means use the default life_time in gcSocket.
			pop_header.keep_alive_time_out = keep_alive_time_out > 2 ? keep_alive_time_out - 2 : -1;
		}
		return KGL_OK;
	}
	case kgl_header_content_length:
	{
		int64_t content_length;
		if (!khttp_parse_length(val, val_len, &content_length) ||
			(pop_header.has_content_length && content_length != pop_header.response_length)) return KGL_EDATA_FORMAT;
		if (pop_header.upstream_is_chunk) pop_header.keep_alive_time_out = -1;
		pop_header.has_content_length=1;
		pop_header.response_length=content_length;
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
			if (rs.bad_name || KGL_OK != PushHeader(rq, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line)) {
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
	if (body.ctx == nullptr) {
		//no body response (HEAD/204/304...), discard the upstream body.
		(*data) += len;
		return KGL_OK;
	}
	KGL_RESULT result = PushBody(rq, &body, *data, (int)len);
	(*data) += len;
	return result;
}
KGL_RESULT KAsyncFetchObject::upstream_is_error(KHttpRequest* rq, int error, const char* msg, bool transport_error) {
	int err = errno;
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)) {
		//return shutdown(rq);
	}
	if (!rq->ctx.read_huped) {
		KBIT_SET(rq->sink->data.flags, RQ_UPSTREAM_ERROR);
		if (client) {
			if (client->IsNew()) {
				//new的才计算错误.
				client->health(transport_error ? HealthStatus::Err : HealthStatus::ResponseErr);
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
