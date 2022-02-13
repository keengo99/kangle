#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "KHttpRequest.h"
#include "http.h"
#include "KAsyncFetchObject.h"
void KTcpUpstream::OnPushContainer()
{
#ifndef _WIN32
	if (cn->st.selector) {
		selectable_remove(&cn->st);
		cn->st.selector = NULL;
	}
	kassert(TEST(cn->st.st_flags, STF_READ | STF_WRITE | STF_REV | STF_WEV) == 0);
#endif
}
void KTcpUpstream::Gc(int life_time,time_t base_time)
{
	if (container == NULL) {
		Destroy();
		return;
	}
#ifndef NDEBUG
	if (cn->st.selector) {
		kassert(cn->st.queue.next == NULL);
	}
#endif
	container->gcSocket(this, life_time, base_time);
}
int KTcpUpstream::Read(char* buf, int len)
{
	return kfiber_net_read(cn, buf, len);
}
int KTcpUpstream::Write(WSABUF* buf, int bc)
{
	return kfiber_net_writev(cn, buf, bc);
}

void KTcpUpstream::BindSelector(kselector *selector)
{
	kassert(cn->st.selector == NULL || cn->st.selector == selector);
	selectable_bind(&cn->st, selector);
}
bool KTcpUpstream::BuildHttpHeader(KHttpRequest *rq, KAsyncFetchObject * fo, KWriteStream *s)
{
	char tmpbuff[50];
	KBridgeStream s2(s, false);
	KStreamHttpEnv env(&s2);
	KHttpHeader *av;
	const char *meth = rq->getMethod();
	if (meth == NULL) {
		return false;
	}
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	int defaultPort = 80;
	if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
		defaultPort = 443;
	}
	KUrl *url = rq->url;
	if (TEST(rq->filter_flags, RF_PROXY_RAW_URL) || !TEST(rq->raw_url.flags, KGL_URL_REWRITED)) {
		url = &rq->raw_url;
	}
	char *path = url->path;
	if (url == rq->url && TEST(url->flags, KGL_URL_ENCODE)) {
		size_t path_len;
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	*s << meth;
	s->WSTRING(" ");
	if (TEST(rq->filter_flags, RF_PROXY_FULL_URL)) {
		if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
			s->WSTRING("https");
		} else {
			s->WSTRING("http");
		}
		s->WSTRING("://");
		*s << url->host;
		if (url->port != defaultPort) {
			s->WSTRING(":");
			*s << url->port;
		}
	}
	*s << path;
	const char *param = url->param;
	if (param) {
		s->WSTRING("?");
		*s << param;
	}
	s->WSTRING(" HTTP/1.1\r\nHost: ");
	*s << url->host;
	if (url->port != defaultPort) {
		s->WSTRING(":");
		*s << url->port;
	}
	s->WSTRING("\r\n");
	av = rq->GetHeader();
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif

		if (strcasecmp(av->attr, X_REAL_IP_SIGN) == 0) {
			goto do_not_insert;
		}
		if (TEST(rq->filter_flags, RF_X_REAL_IP)) {
			if (is_attr(av, "X-Real-IP") || is_attr(av, "X-Forwarded-Proto")) {
				goto do_not_insert;
			}
		}
		if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			x_forwarded_for_inserted = true;
			s->WSTRING("X-Forwarded-For: ");
			*s << av->val;
			s->WSTRING(",");
			*s << rq->getClientIp();
			s->WSTRING("\r\n");
			goto do_not_insert;
		}
		if (is_attr(av, "Via") && TEST(rq->filter_flags, RF_VIA)) {
			if (via_inserted) {
				goto do_not_insert;
			}
			insert_via(rq, s2, av->val);
			via_inserted = true;
			goto do_not_insert;
		}
		if (is_attr(av, kgl_expand_string("Connection"))) {
			goto do_not_insert;
		}
		if (TEST(rq->flags, RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
		//{{ent
#ifdef ENABLE_BIG_OBJECT_206
		if (rq->bo_ctx && is_attr(av, "Range")) {
			goto do_not_insert;
		}
#endif//}}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		s->write_all(NULL, av->attr, av->attr_len);
		s->WSTRING(": ");
		s->write_all(NULL, av->val, av->val_len);
		s->WSTRING("\r\n");
	do_not_insert: av = av->next;
	}
	int64_t content_length = fo->in->f->get_read_left(fo->in, rq);
	if (rq_has_content_length(rq,content_length)) {
		s->WSTRING("Content-Length: ");
		int2string(rq->content_length, tmpbuff);
		*s << tmpbuff;
		s->WSTRING("\r\n");
	} else if (fo->IsChunkPost() || TEST(rq->flags, RQ_INPUT_CHUNKED)) {
		s->WSTRING("Transfer-Encoding: chunked\r\n");
	}
	if (rq->ctx->lastModified != 0) {
		mk1123time(rq->ctx->lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			s->WSTRING("If-Range: ");
		} else {
			s->WSTRING("If-Modified-Since: ");
		}
		*s << tmpbuff;
		s->WSTRING("\r\n");
	} else if (rq->ctx->if_none_match != NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			s->WSTRING("If-Range: ");
		} else {
			s->WSTRING("If-None-Match: ");
		}
		s->write_all(NULL, rq->ctx->if_none_match->data, (int)rq->ctx->if_none_match->len);
		s->WSTRING("\r\n");
	}
	
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx && rq->range_from >= 0) {	
		*s << "Range: bytes=" << rq->range_from << "-";
		if (rq->range_to >= 0) {
			*s << rq->range_to;
		}
		s->WSTRING("\r\n");
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
	//}}
	if (rq->ctx->internal) {
		s->WSTRING("User-Agent: " PROGRAM_NAME "/" VERSION "\r\n");
	}
	if (TEST(rq->flags, RQ_HAS_CONNECTION_UPGRADE)) {
		s->WSTRING("Connection: upgrade\r\n");
	} else if (TEST(rq->filter_flags, RF_UPSTREAM_NOKA) || GetLifeTime() <= 0) {
		s->WSTRING("Connection: close\r\n");
	}
	if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
		*s << "X-Forwarded-For: " << rq->getClientIp();
		s->WSTRING("\r\n");
	}
	if (TEST(rq->filter_flags, RF_VIA) && !via_inserted) {
		insert_via(rq, s2, NULL);
	}
	if (TEST(rq->filter_flags, RF_X_REAL_IP)) {
		s->WSTRING(X_REAL_IP_HEADER);
		s->WSTRING(": ");
		*s << rq->getClientIp();
		s->WSTRING("\r\n");
		if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
			s->WSTRING("X-Forwarded-Proto: https\r\n");
		} else {
			s->WSTRING("X-Forwarded-Proto: http\r\n");
		}
	}
	s->WSTRING("\r\n");
	if (path != url->path) {
		xfree(path);
	}
	return true;
}
