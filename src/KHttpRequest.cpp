/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <errno.h>
#include "kmalloc.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "kthread.h"
#include "http.h"
#include "KVirtualHost.h"
#include "kselector.h"
#include "KHttpKeyValue.h"
#include "KHttpField.h"
#include "KHttpFieldValue.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"
#include "HttpFiber.h"
#include "KBigObjectContext.h"
#include "KHttp2.h"
#include "KSimulateRequest.h"
#include "time_utils.h"
#include "kselector_manager.h"
#include "KFilterContext.h"
#include "KUrlParser.h"
#include "KBufferFetchObject.h"
 //#include "KHttpTransfer.h"
#include "KVirtualHostManage.h"
#include "kselectable.h"
#include "KStaticFetchObject.h"
#include "KAccessDsoSupport.h"
#include "kfiber.h"
#include "KSockPoolHelper.h"
#include "KCache.h"

using namespace std;
namespace kangle {
	inline void destroy_source(KHttpRequestData* rq) {
		while (rq->fo_head) {
			KFetchObject* fo_next = rq->fo_head->next;
			delete rq->fo_head;
			rq->fo_head = fo_next;
		}
		rq->fo_last = nullptr;
	}
	inline void clean_obj(KHttpRequestData* rq) {
		if (rq->ctx.obj) {
			rq->ctx.obj->release();
			rq->ctx.obj = nullptr;
		}
		if (rq->ctx.old_obj) {
			rq->ctx.old_obj->release();
			rq->ctx.old_obj = nullptr;
		}
	}
	inline void store_obj(KHttpRequest* rq) {
		if (rq->ctx.have_stored) {
			return;
		}
		rq->ctx.have_stored = 1;
		//printf("old_obj = %p\n",old_obj);
		if (rq->ctx.old_obj) {
			//send from cache
			assert(rq->ctx.obj);
			KBIT_CLR(rq->ctx.filter_flags, RQ_SWAP_OLD_OBJ);
			if (rq->ctx.obj->data->i.status_code == STATUS_NOT_MODIFIED) {
				//更新obj
				//删除新obj
				assert(rq->ctx.old_obj->in_cache);
				cache.rate(rq->ctx.old_obj);
				return;
			}
			if (rq->ctx.obj->in_cache == 0) {
				if (stored_obj(rq, rq->ctx.obj, rq->ctx.old_obj)) {
					cache.dead(rq->ctx.old_obj, __FILE__, __LINE__);
					return;
				}
			}
			if (KBIT_TEST(rq->ctx.filter_flags, RF_ALWAYS_ONLINE) ||
				(KBIT_TEST(rq->ctx.old_obj->index.flags, OBJ_IS_GUEST) && !KBIT_TEST(rq->ctx.filter_flags, RF_GUEST))
				) {
				//永久在线，并且新网页没有存储
				//或者是会员访问了游客缓存
				//旧网页继续使用
				cache.rate(rq->ctx.old_obj);
			} else {
				cache.dead(rq->ctx.old_obj, __FILE__, __LINE__);
			}
			return;
		}
		if (rq->ctx.obj == NULL) {
			return;
		}
		//check can store
		if (rq->ctx.obj->in_cache == 0) {
			stored_obj(rq, rq->ctx.obj, rq->ctx.old_obj);
		} else {
			assert(rq->ctx.obj->in_cache == 1);
			cache.rate(rq->ctx.obj);
		}
	}
};
kev_result on_sink_readhup(KOPAQUE data, void* arg, int got) {
	/**
	* on_readhup the KOPAQUE data param is no use.
	* Http2 on_readhup data param always is NULL.
	*/
	KHttpRequest* rq = (KHttpRequest*)arg;
	rq->ctx.read_huped = true;
	rq->sink->shutdown();
	KFetchObject* fo_head = rq->fo_head;
	while (fo_head) {
		fo_head->on_readhup(rq);
		fo_head = fo_head->next;
	}
	return kev_ok;
}
KOutputFilterContext* KHttpRequestData::getOutputFilterContext() {
	if (of_ctx == NULL) {
		of_ctx = new KOutputFilterContext;
		return of_ctx;
	}
	return of_ctx;
}

void KHttpRequestData::clean_obj() {
	kangle::clean_obj(this);
}
void KHttpRequestData::push_obj(KHttpObject* obj) {
	assert(ctx.old_obj == NULL);
	if (this->ctx.obj == NULL) {
		this->ctx.obj = obj;
		return;
	}
	this->ctx.old_obj = this->ctx.obj;
	this->ctx.obj = obj;
}
void KHttpRequestData::pop_obj() {
	if (ctx.old_obj) {
		assert(ctx.obj);
		ctx.obj->release();
		ctx.obj = ctx.old_obj;
		ctx.old_obj = NULL;
	}
}
void KHttpRequestData::dead_old_obj() {
	if (ctx.old_obj == NULL) {
		return;
	}
	cache.dead(ctx.old_obj, __FILE__, __LINE__);
	ctx.old_obj->release();
	ctx.old_obj = NULL;
}
void KHttpRequestData::destroy_source() {
	kangle::destroy_source(this);
}
void KHttpRequest::parse_connection(const char* val, const char* end) {
	KHttpFieldValue field(val, end);
	do {
		if (field.is("keep-alive", 10)) {
			ctx.upstream_connection_keep_alive = true;
		} else if (field.is("close", 5)) {
			ctx.upstream_connection_keep_alive = false;
		} else if (KBIT_TEST(sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) && field.is(_KS("upgrade"))) {
			KBIT_SET(sink->data.flags, RQ_CONNECTION_UPGRADE);
			sink->data.left_read = -1;
			ctx.upstream_connection_keep_alive = false;
		}
	} while (field.next());
}
char* KHttpRequest::map_url_path(const char* url, KBaseRedirect* caller) {
	KSubVirtualHost* nsvh = NULL;
	auto svh = kangle::get_virtual_host(this);
	if (svh == NULL) {
		return NULL;
	}
	char* filename = NULL;
	KSafeUrl u(new KUrl());
	const char* path = url;
	if (parse_url(url, u.get()) && u->host != nullptr) {
		path = u->path;
		conf.gvm->queryVirtualHost((KVirtualHostContainer*)sink->get_server_opaque(), &nsvh, u->host, 0);
		if (nsvh && nsvh->vh == svh->vh) {
			//相同的vh
			svh = nsvh;
		}
	}
	bool brd_check_passed = true;
	if (caller != NULL) {
		auto rd = svh->vh->refsPathRedirect(path, (int)strlen(path));
		if (rd != caller) {
			//无权限
			brd_check_passed = false;
		}
		if (rd) {
			rd->release();
		}
	}
	if (brd_check_passed) {
		filename = svh->mapFile(path);
	}
	if (nsvh) {
		nsvh->release();
	}
	return filename;
}
KHttpHeaderIteratorResult handle_http_header(void* arg, KHttpHeader* header) {
	KHttpRequest* rq = (KHttpRequest*)arg;
	if (
#ifdef HTTP_PROXY
	(KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) && kgl_is_attr(header, _KS("Authorization"))) ||
#endif
		kgl_is_attr(header, _KS(AUTH_REQUEST_HEADER))) {
		KBIT_SET(rq->sink->data.flags, AUTH_HAS_FLAG);
#ifdef ENABLE_TPROXY
		if (!KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TPROXY)) {
#endif
			char* end = header->buf + header->val_offset + header->val_len;

			char* p = header->buf + header->val_offset;
			while (*p && !IS_SPACE(*p)) {
				p++;
			}
			char* p2 = p;
			while (*p2 && IS_SPACE(*p2)) {
				p2++;
			}
			KHttpAuth* tauth = NULL;
			if (strncasecmp(header->buf + header->val_offset, "basic", p - (header->buf + header->val_offset)) == 0) {
				tauth = new KHttpBasicAuth;
			} else if (strncasecmp(header->buf + header->val_offset, "digest", p - (header->buf + header->val_offset)) == 0) {
#ifdef ENABLE_DIGEST_AUTH
				tauth = new KHttpDigestAuth;
#endif
			}
			if (tauth) {
				if (!tauth->parse(rq, p2)) {
					delete tauth;
					tauth = NULL;
				}
			}
			if (rq->auth) {
				delete rq->auth;
			}
			rq->auth = tauth;
#ifdef HTTP_PROXY
			return KHttpHeaderIteratorResult::Free;
#endif

#ifdef ENABLE_TPROXY
		}
#endif
		return KHttpHeaderIteratorResult::Continue;
	}
	return KHttpHeaderIteratorResult::Continue;
}
void KHttpRequest::clean() {
	kangle::store_obj(this);
	log_access(this);
	if (of_ctx) {
		delete of_ctx;
		of_ctx = NULL;
	}
	while (slh) {
		KSpeedLimitHelper* slh_next = slh->next;
		delete slh;
		slh = slh_next;
	}
	if (auth) {
		delete auth;
		auth = NULL;
	}
	kangle::destroy_source(this);
	if (file) {
		delete file;
		file = NULL;
	}
	kangle::clean_obj(this);
	/* be sure response_body is closed. */
	assert(ctx.body.ctx == nullptr);
	assert(ctx.in_body == nullptr);
}
void KHttpRequest::store_obj() {
	kangle::store_obj(this);
}
bool KHttpRequest::rewrite_url(const char* newUrl, int errorCode, const char* prefix) {
	KSafeUrl url2(new KUrl());
	if (!parse_url(newUrl, url2.get())) {
		KStringBuf nu;
		if (prefix) {
			if (*prefix != '/') {
				nu << "/";
			}
			nu << prefix;
			int len = (int)strlen(prefix);
			if (len > 0 && prefix[len - 1] != '/') {
				nu << "/";
			}
			nu << newUrl;
		} else {
			char* basepath = strdup(sink->data.url->path);
			char* p = strrchr(basepath, '/');
			if (p) {
				p++;
				*p = '\0';
			}
			nu << basepath;
			free(basepath);
			nu << newUrl;
		}
		if (!parse_url(nu.c_str(), url2.get())) {
			return false;
		}
	}
	if (url2->path == NULL) {
		return false;
	}
	if (KBIT_TEST(url2->flags, KGL_URL_ORIG_SSL)) {
		KBIT_SET(url2->flags, KGL_URL_SSL);
	}
	KStringStream s;
	if (errorCode > 0) {
		s << errorCode << ";";
		if (KBIT_TEST(sink->data.url->flags, KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		s << "://" << sink->data.url->host << ":" << sink->data.url->port << sink->data.url->path;
		if (sink->data.url->param) {
			s << "?" << sink->data.url->param;
		}
	}
	if (url2->host == NULL) {
		url2->host = strdup(sink->data.url->host);
	}
	url_decode(url2->path, 0, url2.get());
	if (ctx.obj && ctx.obj->uk.url == sink->data.url) {// && !KBIT_TEST(ctx->obj->index.flags,FLAG_URL_FREE)) {
		ctx.obj->uk.url->release();
		ctx.obj->uk.url = url2->add_ref();
	}
	sink->data.url->release();
	sink->data.url = url2->add_ref();

	if (errorCode > 0) {
		if (sink->data.url->param) {
			xfree(sink->data.url->param);
		}
		sink->data.url->param = s.steal().release();
	}
	KBIT_SET(sink->data.raw_url->flags, KGL_URL_REWRITED);
	return true;
}
kgl_auto_cstr KHttpRequest::getUrl() {
	if (sink->data.url == NULL) {
		return kgl_auto_cstr(strdup(""));
	}
	return sink->data.url->getUrl();
}
KString KHttpRequest::getInfo() {
	KStringStream s;
#ifdef HTTP_PROXY
	if (sink->data.meth == METH_CONNECT) {
		s << "CONNECT ";
		if (sink->data.raw_url->host) {
			s << sink->data.raw_url->host << ":" << sink->data.raw_url->port;
		}
		return s.str();
	}
#endif
	sink->data.raw_url->GetUrl(s, true);
	return s.str();
}
void KHttpRequest::response_vary(const char* vary) {
	KHttpField field;
	field.parse(vary, ',');
	KStringBuf s;
	http_field_t* head = field.getHeader();
	while (head) {
		if (*head->attr != KGL_VARY_EXTEND_CHAR) {
			if (s.size() > 0) {
				s.write_all(kgl_expand_string(", "));
			}
			s << head->attr;
		} else {
			//vary extend
			char* name = head->attr + 1;
			char* value = strchr(name, '=');
			if (value) {
				*value = '\0';
				value++;
			}
			response_vary_extend(this, &s, name, value);
		}
		head = head->next;
	}
	int len = s.size();
	if (len > 0) {
		response_header(kgl_expand_string("Vary"), s.c_str(), len);
	}
}
kgl_auto_cstr KHttpRequest::build_vary(const char* vary) {
	KHttpField field;
	field.parse(vary, ',');
	KStringStream s;
	http_field_t* head = field.getHeader();
	while (head) {
		if (strcasecmp(head->attr, "Accept-Encoding") != 0) {
			if (*head->attr == KGL_VARY_EXTEND_CHAR) {
				//vary extend
				char* name = head->attr + 1;
				char* value = strchr(name, '=');
				if (value) {
					*value = '\0';
					value++;
				}
				if (!build_vary_extend(this, &s, name, value)) {
					//如果存在不能识别的扩展vary，则不缓存
					KBIT_SET(this->ctx.filter_flags, RF_NO_CACHE);
					return nullptr;
				}
			} else {
				KHttpHeader* rq_header = sink->data.find(head->attr, (int)strlen(head->attr));
				if (rq_header) {
					s.write_all(rq_header->buf + rq_header->val_offset, rq_header->val_len);
				}
			}
			s.WSTR("\n");
		}
		head = head->next;
	}
	if (s.size() == 0) {
		return nullptr;
	}
	return s.steal();
}

void KHttpRequest::append_source(KFetchObject* fo) {
	if (!KBIT_TEST(fo->flags,KGL_UPSTREAM_FILTER) && KBIT_TEST(ctx.filter_flags, RQ_NO_EXTEND) && !KBIT_TEST(sink->data.flags, RQ_IS_ERROR_PAGE)) {
		//无扩展处理
		if (KBIT_TEST(fo->flags,KGL_UPSTREAM_NEED_QUEUE)) {
			delete fo;
			fo = new KStaticFetchObject();
		}
	}
	if (fo_last) {
		fo_last->next = fo;
	} else {
		assert(fo_head == nullptr);
		fo_head = fo;
	}
	fo_last = fo;
}
KFetchObject* KHttpRequest::replace_next(KFetchObject* fo, KFetchObject* next_fo) {
	assert(next_fo);
	next_fo = nullptr;
	assert(fo->next != nullptr || fo == fo_last);
	KFetchObject* old_next = fo->next;
	fo->next = next_fo;
	fo_last = next_fo;
	return old_next;
}
KGL_RESULT KHttpRequest::open_next(KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, const char* queue) {
	KBIT_SET(sink->data.flags, RQ_NEXT_CALLED);
	KGL_RESULT result;
	for (;;) {
		fo = fo->next;
		if (!fo) {
			return KGL_ENOT_READY;
		}
#ifdef ENABLE_REQUEST_QUEUE
		if (queue) {
			ReleaseQueue();
			this->queue = get_request_queue(this, queue);
			result = open_queued_fetchobj(this, fo, in, out, this->queue);
		} else
#endif
			result = fo->Open(this, in, out);
		if (KBIT_TEST(sink->data.flags, RQ_HAS_SEND_HEADER | RQ_HAS_READ_POST)) {
			return result;
		}
		if (result != KGL_NEXT) {
			return result;
		}
	}
}
