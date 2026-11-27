/*
 * KHttpProxyFetchObject.cpp
 *
 *  Created on: 2010-4-20
 *      Author: keengo
 */
#include "do_config.h"
#include "KHttpProxyFetchObject.h"
#include "http.h"
#include "kmd5.h"
#include "log.h"
#include "KHttpResponseParser.h"
#include "KPoolableSocketContainer.h"
#include "KRewriteMarkEx.h"
#include "kmalloc.h"
#include "KSimulateRequest.h"
#include "KHttpFieldValue.h"
#include "KHttpValidation.h"
#include "KHttpLib.h"
#ifdef KSOCKET_SSL
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

static bool is_connection_option(KHttpRequest* rq, const kgl_str_t& attr)
{
	for (KHttpHeader* header = rq->sink->data.get_header(); header; header = header->next) {
		if (!kgl_is_attr(header, _KS("Connection"))) {
			continue;
		}
		const char* value = header->buf + header->val_offset;
		KHttpFieldValue field(value, value + header->val_len);
		do {
			const char* field_end = field.get_field_end();
			while (field_end > field.val && isspace((unsigned char)field_end[-1])) {
				field_end--;
			}
			if ((size_t)(field_end - field.val) == attr.len &&
				kgl_mem_case_same(field.val, attr.len, attr.data, attr.len)) {
				return true;
			}
		} while (field.next());
	}
	return false;
}

bool http2_header_callback(KUpstream *us, void *arg, const char *attr, int attr_len, const char *val, int val_len,bool flag)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	/* only KHttpProxyFetchObject::buildHead installs this callback */
	KHttpProxyFetchObject *fo = static_cast<KHttpProxyFetchObject *>((KAsyncFetchObject *)us->GetOpaque());
	assert(kselector_is_same_thread(rq->sink->get_selector()));
	return fo->push_upstream_header(rq, attr, attr_len, val, val_len, flag);
}
bool KHttpProxyFetchObject::push_upstream_header(KHttpRequest* rq, const char* attr, int attr_len, const char* val, int val_len, bool request_line)
{
	if (ws_translate == KGL_WS_TO_H1 && khttp_field_equal(attr, attr_len, "sec-websocket-accept")) {
		/* HTTP/2 WebSockets do not use it (RFC 8441 section 5) */
		return true;
	}
	if (KGL_OK != PushHeader(rq, attr, attr_len, val, val_len, request_line)) {
		return false;
	}
#ifdef KSOCKET_SSL
	/* KHttp2 adds Upgrade only when the origin accepted the extended CONNECT */
	if (ws_translate == KGL_WS_TO_H2 && !ws_key.empty() && khttp_field_equal(attr, attr_len, "upgrade")) {
		static const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string src = ws_key + guid;
		unsigned char digest[SHA_DIGEST_LENGTH];
		SHA1((const unsigned char*)src.data(), src.size(), digest);
		std::string accept = b64encode(digest, (int)sizeof(digest));
		ws_key.clear();
		if (KGL_OK != PushHeader(rq, _KS("Sec-WebSocket-Accept"), accept.c_str(), (int)accept.size(), false)) {
			return false;
		}
	}
#endif
	return true;
}
bool upstream_sign_request(KHttpRequest *rq, KHttpEnv *s)
{
	KStringBuf v;
	if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
		v.WSTR("p=https,");
	}
	v.WSTR("sp=");
	v << rq->GetSelfPort();
	if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
		v.WSTR("s");
	}
	v.WSTR(",ip=");
	v << rq->getClientIp();
	v.WSTR(",t=");
	v << (int)kgl_current_sec;
	char buf[33];
	KMD5_CTX context;
	unsigned char digest[17];
	KMD5Init(&context);
	KMD5Update(&context, (unsigned char *)v.buf(), v.size());
	int upstream_sign_len = conf.upstream_sign_len;
	if (upstream_sign_len > (int)sizeof(conf.upstream_sign)) {
		upstream_sign_len = sizeof(conf.upstream_sign);
	}
	KMD5Update(&context, (unsigned char *)conf.upstream_sign, upstream_sign_len);
	KMD5Final(digest, &context);
	make_digest(buf, digest);
	v.WSTR("|");
	v.write_all(buf, 32);
	return s->add(kgl_expand_string(X_REAL_IP_SIGN), v.buf(), v.size());
}
KGL_RESULT KHttpProxyFetchObject::buildHead(KHttpRequest *rq)
{	
	client->set_header_callback(rq, http2_header_callback);
	assert(buffer == NULL);
	buffer = new KSocketBuffer(16384);
	if (client->IsMultiStream()) {
		pop_header.proto = Proto_spdy;
	} else {
		pop_header.proto = Proto_http;
	}
	if (!build_http_header(rq)) {
		return KGL_ECAN_RETRY_SOCKET_BROKEN;
	}
	return client->send_header_complete();
}
bool KHttpProxyFetchObject::build_http_header(KHttpRequest* rq)
{
	char tmpbuff[50];
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	KStringBuf s;
	KUpstreamStreamHttpEnv env(client);
	const char* ips = rq->getClientIp();
	int ips_len = (int)strlen(ips);
	KUrl* url = rq->sink->data.url;
	if (KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_RAW_URL) || !KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_REWRITED)) {
		url = &rq->sink->data.raw_url;
	}
#ifdef HTTP_PROXY
	if (rq->sink->data.meth == METH_CONNECT) {
		build_url_host_port(url, s);
		if (!client->send_method_path(METH_CONNECT, s.buf(), (hlen_t)s.size())) {
			return false;
		}
	} else {
#endif
		char* path = url->path;
		size_t path_len;
		if (url == rq->sink->data.url && KBIT_TEST(url->flags, KGL_URL_ENCODE)) {
			path = url_encode(url->path, strlen(url->path), &path_len);
		}
		if (KBIT_TEST(rq->ctx.filter_flags, RF_PROXY_FULL_URL) && !client->IsMultiStream()) {
			if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
				s.WSTR("https");
			} else {
				s.WSTR("http");
			}
			s.WSTR("://");
			build_url_host_port(url, s);
		}
		s << path;
		const char* param = url->param;
		if (param) {
			s.WSTR("?");
			s << param;
		}
		uint8_t meth = rq->sink->data.meth;
		if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) && client->IsMultiStream()) {
			meth = METH_CONNECT;
		}
		bool method_sent = client->send_method_path(meth, s.buf(), (hlen_t)s.size());
		if (path != url->path) {
			xfree(path);
			path = NULL;
		}
		if (!method_sent) {
			return false;
		}
		s.clear();
		build_url_host_port(url, s);
		if (!client->send_host(s.buf(), (hlen_t)s.size())) {
			return false;
		}
		if (client->IsMultiStream()) {
			if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
				if (!client->send_header(kgl_header_scheme, kgl_expand_string("https"))) {
					return false;
				}
			} else {
				if (!client->send_header(kgl_header_scheme, kgl_expand_string("http"))) {
					return false;
				}
			}
		}
#ifdef HTTP_PROXY
	}
#endif	
	KHttpHeader* av = rq->sink->data.get_header();
	ws_translate = 0;
	ws_key.clear();
#ifdef KSOCKET_SSL
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) &&
		(rq->sink->data.http_version >= 0x200) != client->IsMultiStream()) {
		/* client and origin speak different WebSocket handshakes */
		bool websocket = false;
		for (auto h = av; h; h = h->next) {
			if (kgl_is_attr(h, _KS("Upgrade"))) {
				websocket = kgl_mem_case_same(h->buf + h->val_offset, h->val_len, _KS("websocket"));
			} else if (kgl_is_attr(h, _KS("Sec-WebSocket-Key"))) {
				ws_key.assign(h->buf + h->val_offset, h->val_len);
			}
		}
		if (websocket) {
			ws_translate = client->IsMultiStream() ? KGL_WS_TO_H2 : KGL_WS_TO_H1;
		}
	}
#endif
	if (client->IsMultiStream() && KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		for (auto h=av; h; h=h->next) {
			if (kgl_is_attr(h, _KS("Upgrade")) && !client->send_header(kgl_header_upgrade,h->buf+h->val_offset,h->val_len)) return false;
		}
	}
	kgl_str_t attr;
	while (av) {
		kgl_get_header_name(av, &attr);
		if (kgl_is_attr(av, _KS("Keep-Alive")) ||
			(client->IsMultiStream() && kgl_is_attr(av, _KS("Upgrade"))) ||
			kgl_is_attr(av, _KS("Proxy-Connection")) ||
			kgl_is_attr(av, _KS("Connection")) ||
			(!client->IsMultiStream() && kgl_is_attr(av, _KS("TE"))) ||
			(!client->IsMultiStream() && !pop_header.post_is_chunk && kgl_is_attr(av, _KS("Trailer"))) ||
			kgl_is_attr(av, _KS("Transfer-Encoding")) ||
			(!KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) && kgl_is_attr(av, _KS("Upgrade"))) ||
			(is_connection_option(rq, attr) &&
				!(KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) && kgl_is_attr(av, _KS("Upgrade")))) ||
			(!av->name_is_know && *(av->buf)==':') ||
			(ws_translate == KGL_WS_TO_H2 && kgl_is_attr(av, _KS("Sec-WebSocket-Key")))) {
			goto do_not_insert;
		}
#ifdef HTTP_PROXY
		if (kgl_ncasecmp(attr.data, attr.len, _KS("Proxy-")) == 0) {
			goto do_not_insert;
		}
#endif
		if (kgl_is_attr(av, _KS(X_REAL_IP_SIGN))) {
			goto do_not_insert;
		}
		if (KBIT_TEST(rq->ctx.filter_flags, RF_X_REAL_IP) && (kgl_is_attr(av, _KS("X-Real-IP")) || kgl_is_attr(av, _KS("X-Forwarded-Proto")))) {
			goto do_not_insert;
		}
		if (!KBIT_TEST(rq->ctx.filter_flags, RF_NO_X_FORWARDED_FOR) && kgl_is_attr(av, _KS("X-Forwarded-For"))) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			x_forwarded_for_inserted = true;
			s.clear();
			s.write_all(av->buf+av->val_offset, av->val_len);
			s.write_all(_KS(","));
			s << ips;
			if (!client->send_header(kgl_expand_string("X-Forwarded-For"), s.buf(), (hlen_t)s.size())) {
				return false;
			}
			goto do_not_insert;
		}
		if (kgl_is_attr(av, _KS("Via")) && KBIT_TEST(rq->ctx.filter_flags, RF_VIA)) {
			if (via_inserted) {
				goto do_not_insert;
			}
			via_inserted = true;
			s.clear();
			insert_via(rq, s, av->buf+av->val_offset, av->val_len);
			if (!client->send_header(kgl_expand_string("Via"), s.buf(), (hlen_t)s.size())) {
				return false;
			}
			goto do_not_insert;
		}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		if (av->name_is_know) {
			if (!client->send_header((kgl_header_type)av->know_header, av->buf + av->val_offset, av->val_len)) {
				return false;
			}
		} else if (!client->send_header(av->buf, av->name_len, av->buf+av->val_offset, av->val_len)) {
			return false;
		}
	do_not_insert: 
		av = av->next;
	}
	
	int64_t content_length = in->f->body.get_left(in->body_ctx);
	if (pop_header.post_is_chunk) content_length = -1;
	if (rq_has_content_length(rq, content_length)) {
		int len = int2string2(content_length, tmpbuff);
		if (!client->send_header(kgl_expand_string("Content-Length"), tmpbuff, len)) {
			return false;
		}
	} else {	
		if (pop_header.post_is_chunk) {
			assert(content_length == -1);
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
			content_length = 0;
		}
	}
	client->set_content_length(content_length);
	kgl_precondition_flag flag;
	kgl_precondition* condition = in->f->get_precondition(in->ctx, &flag);
	if (condition && condition->entity) {
		if (KBIT_TEST(flag,kgl_precondition_if_time)) {
			char* end = make_http_time(condition->time, tmpbuff, sizeof(tmpbuff));
			if (KBIT_TEST(flag, kgl_precondition_if_match_unmodified)) {
				if (!client->send_header(kgl_header_if_unmodified_since, tmpbuff, (hlen_t)(end - tmpbuff))) return false;
			} else {
				if (!client->send_header(kgl_header_if_modified_since, tmpbuff, (hlen_t)(end - tmpbuff))) return false;
			}
		} else {
			if (KBIT_TEST(flag, kgl_precondition_if_match_unmodified)) {
				if (!client->send_header(kgl_header_if_match, condition->entity->data, (int)condition->entity->len)) return false;
			} else {
				if (!client->send_header(kgl_header_if_none_match, condition->entity->data, (int)condition->entity->len)) return false;
			}
		}
	}
	kgl_request_range* range = in->f->get_range(in->ctx);
	if (range) {
		s.clear();
		s << "bytes=";
		if (range->from >= 0) {
			s << range->from << "-";
			if (range->to >= 0) {
				s << range->to;
			}
		} else {
			s << range->from;
		}
		if (!client->send_header(kgl_header_range, s.buf(), s.size())) return false;
		if (range->if_range_entity) {
			if (KBIT_TEST(flag, kgl_precondition_if_range_date)) {
				char* end = make_http_time(range->if_range_date, tmpbuff, sizeof(tmpbuff));
				if (!client->send_header(kgl_header_if_range, tmpbuff, (hlen_t)(end - tmpbuff))) return false;
			} else {
				/* if-range not allowed weak etag */
				bool weak_etag = range->if_range_entity->len >= 2 &&
					(range->if_range_entity->data[0] == 'W' || range->if_range_entity->data[0] == 'w') &&
					range->if_range_entity->data[1] == '/';
				if (!weak_etag && !client->send_header(kgl_header_if_range, range->if_range_entity->data, (hlen_t)range->if_range_entity->len)) return false;
			}
		}
	}
#ifdef HTTP_PROXY
	{
		auto container = client->get_container();
		if (container) {
			KHttpHeader* header = container->get_proxy_header(rq->sink->pool);
			while (header) {
				kgl_get_header_name(header, &attr);
				if (!env.add(attr.data, (hlen_t)attr.len, header->buf + header->val_offset, header->val_len)) return false;
				header = header->next;
			}
		}
	}
#endif
	if (rq->ctx.upstream_sign) {
		if (!upstream_sign_request(rq, &env)) return false;
	}
	if (rq->ctx.internal) {
		if (!client->send_header(kgl_expand_string("User-Agent"), kgl_expand_string(PROGRAM_NAME "/" VERSION))) return false;
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
#ifdef KSOCKET_SSL
		if (ws_translate == KGL_WS_TO_H1 && ws_key.empty()) {
			/* HTTP/2 clients send no key, but HTTP/1.1 origins require one */
			unsigned char nonce[16];
			if (RAND_bytes(nonce, sizeof(nonce)) != 1) return false;
			std::string key = b64encode(nonce, (int)sizeof(nonce));
			if (!client->send_header(_KS("Sec-WebSocket-Key"), key.c_str(), (hlen_t)key.size())) return false;
		}
#endif
		if (!client->send_connection(kgl_expand_string("upgrade"))) return false;
	} else if (KBIT_TEST(rq->ctx.filter_flags, RF_UPSTREAM_NOKA) || client->GetLifeTime() <= 0) {
		if (!client->send_connection(kgl_expand_string("close"))) return false;
	}
	if (!KBIT_TEST(rq->ctx.filter_flags, RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
		if (!client->send_header(kgl_expand_string("X-Forwarded-For"), ips, ips_len)) return false;
	}
	if (KBIT_TEST(rq->ctx.filter_flags, RF_VIA) && !via_inserted) {
		s.clear();
		insert_via(rq, s, NULL);
		if (!client->send_header(kgl_expand_string("Via"), s.buf(), (hlen_t)s.size())) {
			return false;
		}
	}
	if (KBIT_TEST(rq->ctx.filter_flags, RF_X_REAL_IP)) {
		if (!client->send_header(kgl_expand_string(X_REAL_IP_HEADER), ips, ips_len)) return false;
		if (!client->IsMultiStream()) {
			if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
				if (!client->send_header(kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("https"))) return false;
			} else {
				if (!client->send_header(kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("http"))) return false;
			}
		}
	}
	return true;
}
