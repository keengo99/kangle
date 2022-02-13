#include "KHttp2Upstream.h"
#include "KHttpRequest.h"
#include "KStringBuf.h"
#include "http.h"
#include "KAsyncFetchObject.h"
#ifdef ENABLE_UPSTREAM_HTTP2
class KHttp2Env : public KHttpEnv {
public:
	KHttp2Env(KHttp2Upstream *us)
	{
		this->us = us;
	}
	bool add(const char *name, hlen_t name_len, const char *val, hlen_t val_len)
	{		
		us->http2->add_header(us->ctx,name, name_len, val, val_len);
		return true;	
	}
private:
	KHttp2Upstream *us;
};
bool KHttp2Upstream::SetHttp2Parser(void *arg, kgl_header_callback header)
{
	assert(kselector_is_same_thread(http2->c->st.selector));
	assert(!ctx->in_closed);
	assert(ctx->read_wait == NULL);
	kgl_http2_event *e = new kgl_http2_event;
	e->header_arg = arg;
	e->header = header;
	ctx->read_wait = e;
	return true;
}
bool KHttp2Upstream::ReadHttpHeader(KGL_RESULT *result)
{
	if (ctx->send_header) {
		http2->send_header(ctx);
	}
	if (0 == http2->ReadHeader(ctx)) {
		*result = KGL_OK;
	} else {
		*result = KGL_EUNKNOW;
	}
	return true;
}
bool KHttp2Upstream::BuildHttpHeader(KHttpRequest *rq, KAsyncFetchObject *fo, KWriteStream *out)
{
	http2->add_method(ctx, rq->meth);
	const char *ips = rq->getClientIp();
	int ips_len = strlen(ips);
	char tmpbuff[50];
	KUrl *url = rq->url;
	if (TEST(rq->filter_flags, RF_PROXY_RAW_URL) || !TEST(rq->raw_url.flags, KGL_URL_REWRITED)) {
		url = &rq->raw_url;
	}
	char *path = url->path;
	size_t path_len = 0;
	if (url == rq->url && TEST(url->flags, KGL_URL_ENCODE)) {
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	KStringBuf s;
	s << path;
	const char *param = url->param;
	if (param) {
		s << "?" << param;
	}
	http2->add_header(ctx,kgl_expand_string(":path"), s.getBuf(), s.getSize());
	if (path != url->path) {
		xfree(path);
	}
	http2->add_header(ctx, kgl_expand_string(":authority"), url->host, (hlen_t)strlen(url->host));
	if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
		http2->add_header(ctx, kgl_expand_string(":scheme"), kgl_expand_string("https"));
	} else {
		http2->add_header(ctx, kgl_expand_string(":scheme"), kgl_expand_string("http"));
	}
	KHttpHeader *av = rq->GetHeader();
	bool x_forwarded_for_inserted = false;
	while (av) {
		if (attr_casecmp(av->attr, "Connection") == 0 ||
			attr_casecmp(av->attr, "Keep-Alive") == 0 ||
			attr_casecmp(av->attr, "Proxy-Connection") == 0 ||
			*av->attr==':' ||
			attr_casecmp(av->attr, "Transfer-Encoding") == 0) {
			goto do_not_insert;
		}
#ifdef ENABLE_DELTA_DECODE
		if (TEST(rq->filter_flags, RF_DELTA) && attr_casecmp(av->attr, "Accept-Encoding") == 0) {
			goto do_not_insert;
		}
#endif
		if (strcasecmp(av->attr, X_REAL_IP_SIGN) == 0) {
			goto do_not_insert;
		}
		if (TEST(rq->filter_flags, RF_X_REAL_IP) && (is_attr(av, "X-Real-IP") || is_attr(av, "X-Forwarded-Proto"))) {
			goto do_not_insert;
		}
		if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			x_forwarded_for_inserted = true;
			KStringBuf s;
			s << av->val << "," << ips;
			http2->add_header(ctx, av->attr, av->attr_len, s.getBuf(), s.getSize());
			goto do_not_insert;
		}
		if (TEST(rq->flags, RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
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
		http2->add_header(ctx, av->attr, av->attr_len, av->val, av->val_len);
	do_not_insert: av = av->next;
	}
	int64_t content_length = fo->in->f->get_read_left(fo->in, rq);
	int len;
	if (rq_has_content_length(rq, content_length)) {
		len = snprintf(tmpbuff, sizeof(tmpbuff) - 1, INT64_FORMAT, content_length);
		http2->add_header(ctx, kgl_expand_string("Content-Length"), tmpbuff,len);
	}
	ctx->SetContentLength(content_length);
	if (rq->ctx->lastModified != 0) {
		len = make_http_time(rq->ctx->lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			http2->add_header(ctx, kgl_expand_string("If-Range"), tmpbuff, len);
		} else {
			http2->add_header(ctx, kgl_expand_string("If-Modified-Since"), tmpbuff, len);
		}
	} else if (rq->ctx->if_none_match != NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			http2->add_header(ctx, kgl_expand_string("If-Range"), rq->ctx->if_none_match->data, (hlen_t)rq->ctx->if_none_match->len);
		} else {
			http2->add_header(ctx, kgl_expand_string("If-None-Match"), rq->ctx->if_none_match->data, (hlen_t)rq->ctx->if_none_match->len);
		}
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		assert(rq->bo_ctx);
		if (rq->range_from >= 0) {
			KStringBuf s;
			s << "bytes=" << rq->range_from << "-";
			if (rq->range_to >= 0) {
				s << rq->range_to;
			}
			http2->add_header(ctx, kgl_expand_string("Range"), s.getBuf(), s.getSize());
		}
	}
#endif
	if (rq->ctx->upstream_sign) {
		KHttp2Env spdy_env(this);
		upstream_sign_request(rq, &spdy_env);
	}
#ifdef ENABLE_SIMULATE_HTTP
	if (!rq->ctx->simulate) {
#endif
		if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
			http2->add_header(ctx, kgl_expand_string("X-Forwarded-For"), ips, ips_len);
		}
		if (TEST(rq->filter_flags, RF_X_REAL_IP)) {
			http2->add_header(ctx, kgl_expand_string("X-Real-IP"), ips, ips_len);
			if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
				http2->add_header(ctx, kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("https"));
			} else {
				http2->add_header(ctx, kgl_expand_string("X-Forwarded-Proto"), kgl_expand_string("http"));
			}
		}
#ifdef ENABLE_SIMULATE_HTTP
	}
#endif
	return true;
}
KUpstream *KHttp2Upstream::NewStream()
{
	kassert(this->ctx->admin_stream);
	return http2->connect();
}
#endif

