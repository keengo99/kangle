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


bool http2_header_callback(KUpstream *us, void *arg, const char *attr, int attr_len, const char *val, int val_len,bool is_first)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = (KAsyncFetchObject *)us->GetOpaque();
	assert(kselector_is_same_thread(rq->sink->get_selector()));
	if (KGL_OK != fo->PushHeader(rq, attr, attr_len, val, val_len, is_first)) {
		return false;
	}
	return true;
}
void upstream_sign_request(KHttpRequest *rq, KHttpEnv *s)
{
	KStringBuf v;
	if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
		v.WSTR("p=https,");
	}
	v.WSTR("sp=");
	v << rq->GetSelfPort();
	if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
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
	KMD5Update(&context, (unsigned char *)v.getBuf(), v.getSize());
	int upstream_sign_len = conf.upstream_sign_len;
	if (upstream_sign_len > (int)sizeof(conf.upstream_sign)) {
		upstream_sign_len = sizeof(conf.upstream_sign);
	}
	KMD5Update(&context, (unsigned char *)conf.upstream_sign, upstream_sign_len);
	KMD5Final(digest, &context);
	make_digest(buf, digest);
	v.WSTR("|");
	v.write_all(buf, 32);
	s->add(kgl_expand_string(X_REAL_IP_SIGN), v.getBuf(), v.getSize());
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
	build_http_header(rq);
	return client->send_header_complete();
}
bool KHttpProxyFetchObject::build_http_header(KHttpRequest* rq)
{
	char tmpbuff[50];
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	int defaultPort = 80;
	KStringBuf s;
	KUpstreamStreamHttpEnv env(client);
	const char* ips = rq->getClientIp();
	int ips_len = (int)strlen(ips);
	if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
		defaultPort = 443;
	}
	KUrl* url = rq->sink->data.url;
	if (KBIT_TEST(rq->filter_flags, RF_PROXY_RAW_URL) || !KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_REWRITED)) {
		url = rq->sink->data.raw_url;
	}
	char* path = url->path;
	size_t path_len;
	if (url == rq->sink->data.url && KBIT_TEST(url->flags, KGL_URL_ENCODE)) {
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	if (KBIT_TEST(rq->filter_flags, RF_PROXY_FULL_URL) && !client->IsMultiStream()) {
		if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
			s.WSTR("https");
		} else {
			s.WSTR("http");
		}
		s.WSTR("://");
		s << url->host;
		if (url->port != defaultPort) {
			s.WSTR(":");
			s << url->port;
		}
	}
	s << path;
	const char* param = url->param;
	if (param) {
		s.WSTR("?");
		s << param;
	}
	client->send_method_path(rq->sink->data.meth, s.getBuf(), (hlen_t)s.getSize());
	if (path != url->path) {
		xfree(path);
		path = NULL;
	}
	s.clean();	
	s << url->host;
	if (url->port != defaultPort) {
		s.WSTR(":");
		s << url->port;
	}
	client->send_host(s.getBuf(), (hlen_t)s.getSize());
	if (client->IsMultiStream()) {
		if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
			client->send_header(kgl_expand_string(":scheme"), kgl_expand_string("https"));
		} else {
			client->send_header(kgl_expand_string(":scheme"), kgl_expand_string("http"));
		}
	}
	KHttpHeader* av = rq->sink->data.GetHeader();
	while (av) {
		if (attr_casecmp(av->attr, "Connection") == 0 ||
			attr_casecmp(av->attr, "Keep-Alive") == 0 ||
			attr_casecmp(av->attr, "Proxy-Connection") == 0 ||
			*av->attr == ':' ||
			attr_casecmp(av->attr, "Transfer-Encoding") == 0) {
			goto do_not_insert;
		}
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif

		if (strcasecmp(av->attr, X_REAL_IP_SIGN) == 0) {
			goto do_not_insert;
		}
		if (KBIT_TEST(rq->filter_flags, RF_X_REAL_IP) && (is_attr(av, "X-Real-IP") || is_attr(av, "X-Forwarded-Proto"))) {
			goto do_not_insert;
		}
		if (!KBIT_TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			x_forwarded_for_inserted = true;
			s.clean();
			s << av->val << "," << ips;
			if (!client->send_header(kgl_expand_string("X-Forwarded-For"), s.getBuf(), (hlen_t)s.getSize())) {
				return false;
			}
			goto do_not_insert;
		}
		if (is_attr(av, "Via") && KBIT_TEST(rq->filter_flags, RF_VIA)) {
			if (via_inserted) {
				goto do_not_insert;
			}
			via_inserted = true;
			s.clean();
			insert_via(rq, s, av->val);
			if (!client->send_header(kgl_expand_string("Via"), s.getBuf(), (hlen_t)s.getSize())) {
				return false;
			}
			goto do_not_insert;
		}
		if (KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
#ifdef ENABLE_BIG_OBJECT_206
		if (rq->bo_ctx && is_attr(av, "Range")) {
			goto do_not_insert;
		}
#endif
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		if (!client->send_header(av->attr, av->attr_len, av->val, av->val_len)) {
			return false;
		}
	do_not_insert: 
		av = av->next;
	}
	int64_t content_length = in->f->get_read_left(in, rq);
	if (rq_has_content_length(rq, content_length)) {
		int len = int2string2(content_length, tmpbuff);
		client->send_header(kgl_expand_string("Content-Length"), tmpbuff, len);
	} else if (IsChunkPost() || KBIT_TEST(rq->sink->data.flags, RQ_INPUT_CHUNKED)) {
		assert(content_length == -1);
	}
	client->set_content_length(content_length);

	if (rq->sink->data.if_modified_since != 0) {
		int len = make_http_time(rq->sink->data.if_modified_since, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			client->send_header(kgl_expand_string("If-Range"), tmpbuff, len);
		} else {
			client->send_header(kgl_expand_string("If-Modified-Since"), tmpbuff, len);
		}		
	} else if (rq->sink->data.if_none_match != NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			client->send_header(kgl_expand_string("If-Range"), rq->sink->data.if_none_match->data, (int)rq->sink->data.if_none_match->len);
		} else {
			client->send_header(kgl_expand_string("If-None-Match"), rq->sink->data.if_none_match->data, (int)rq->sink->data.if_none_match->len);
		}
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx && rq->sink->data.range_from >= 0) {
		s.clean();
		s << "bytes=" << rq->sink->data.range_from << "-";
		if (rq->sink->data.range_to >= 0) {
			s << rq->sink->data.range_to;
		}
		client->send_header(kgl_expand_string("Range"), s.getBuf(), s.getSize());
	}
#endif
#ifdef HTTP_PROXY
	if (container) {
		container->addHeader(rq, &env);
	}
#endif
	if (rq->ctx->upstream_sign) {
		upstream_sign_request(rq, &env);
	}
	if (rq->ctx->internal) {
		client->send_header(kgl_expand_string("User-Agent"), kgl_expand_string(PROGRAM_NAME "/" VERSION));
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		client->send_connection(kgl_expand_string("upgrade"));
	} else if (KBIT_TEST(rq->filter_flags, RF_UPSTREAM_NOKA) || client->GetLifeTime() <= 0) {
		client->send_connection(kgl_expand_string("close"));
	}
	if (!KBIT_TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
		client->send_header(kgl_expand_string("X-Forwarded-For"), ips, ips_len);
	}
	if (KBIT_TEST(rq->filter_flags, RF_VIA) && !via_inserted) {
		s.clean();
		insert_via(rq, s, NULL);
		if (!client->send_header(kgl_expand_string("Via"), s.getBuf(), (hlen_t)s.getSize())) {
			return false;
		}
	}
	if (KBIT_TEST(rq->filter_flags, RF_X_REAL_IP)) {
		client->send_header(kgl_expand_string(X_REAL_IP_HEADER), ips, ips_len);
		if (!client->IsMultiStream()) {
			if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_SSL)) {
				client->send_header(kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("https"));
			} else {
				client->send_header(kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("http"));
			}
		}
	}
	return true;
}