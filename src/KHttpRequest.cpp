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

void WINAPI free_auto_memory(void* arg) {
	xfree(arg);
}
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
KHttpRequestData::~KHttpRequestData() {
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
	destroy_source();
	if (file) {
		delete file;
		file = NULL;
	}
	clean_obj();
	/* be sure response_body is closed. */
	assert(ctx.body.ctx == nullptr);
	assert(ctx.in_body == nullptr);
}
void KHttpRequestData::clean_obj() {
	if (ctx.obj) {
		ctx.obj->release();
		ctx.obj = nullptr;
	}
	if (ctx.old_obj) {
		ctx.old_obj->release();
		ctx.old_obj = nullptr;
	}
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
	while (fo_head) {
		KFetchObject* fo_next = fo_head->next;
		delete fo_head;
		fo_head = fo_next;
	}
	fo_last = nullptr;
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
void KHttpRequest::readhup() {
	if (!conf.read_hup) {
		return;
	}
	sink->readhup(this, on_sink_readhup);
}
char* KHttpRequest::map_url_path(const char* url, KBaseRedirect* caller) {
	KSubVirtualHost* nsvh = NULL;
	auto svh = get_virtual_host();
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
void KHttpRequest::LeaveRequestQueue() {
	sink->set_state(STATE_SEND);
}
void KHttpRequest::EnterRequestQueue() {
	sink->set_state(STATE_WAIT);
}
void KHttpRequest::SetSelfPort(uint16_t port, bool ssl) {
	sink->set_self_port(port, ssl);
}
void KHttpRequest::close_source() {
	destroy_source();
#ifdef ENABLE_REQUEST_QUEUE
	ReleaseQueue();
#endif
}
#ifdef ENABLE_REQUEST_QUEUE
void KHttpRequest::ReleaseQueue() {
	if (queue) {		
		queue->release();
		queue = NULL;
	}
}
#endif
void KHttpRequest::set_url_param(char* param) {
	assert(sink->data.url->param == NULL);
	sink->data.url->param = xstrdup(param);
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
void KHttpRequest::beginRequest() {
	if (sink->data.url->path) {
		KFileName::tripDir3(sink->data.url->path, '/');
	}
#ifdef MALLOCDEBUG
	if (quit_program_flag != PROGRAM_NO_QUIT) {
		KBIT_SET(sink->data.flags, RQ_CONNECTION_CLOSE);
	}
#endif
	sink->data.iterator(handle_http_header, this);
}
uint32_t KHttpRequest::get_upstream_flags() {
	uint32_t flags = 0;
	if (KBIT_TEST(sink->data.flags, RQ_UPSTREAM_ERROR)) {
		KBIT_SET(flags, KSOCKET_FLAGS_SKIP_POOL);
	}
	if (KBIT_TEST(sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		KBIT_SET(flags, KSOCKET_FLAGS_WEBSOCKET);
	}
	return flags;
}
int KHttpRequest::EndRequest() {
	sink->remove_readhup();
	store_obj();
	log_access(this);
	if (ctx.in_body) {
		ctx.in_body->f->close(ctx.in_body->ctx);
		ctx.in_body = nullptr;
	}
	delete this;
	return 0;
}

void KHttpRequest::store_obj() {
	if (ctx.have_stored) {
		return;
	}
	ctx.have_stored = 1;
	//printf("old_obj = %p\n",old_obj);
	if (ctx.old_obj) {
		//send from cache
		assert(ctx.obj);
		KBIT_CLR(ctx.filter_flags, RQ_SWAP_OLD_OBJ);
		if (ctx.obj->data->i.status_code == STATUS_NOT_MODIFIED) {
			//更新obj
			//删除新obj
			assert(ctx.old_obj->in_cache);
			cache.rate(ctx.old_obj);
			return;
		}
		if (ctx.obj->in_cache == 0) {
			if (stored_obj(this, ctx.obj, ctx.old_obj)) {
				cache.dead(ctx.old_obj, __FILE__, __LINE__);
				return;
			}
		}
		if (KBIT_TEST(ctx.filter_flags, RF_ALWAYS_ONLINE) ||
			(KBIT_TEST(ctx.old_obj->index.flags, OBJ_IS_GUEST) && !KBIT_TEST(ctx.filter_flags, RF_GUEST))
			) {
			//永久在线，并且新网页没有存储
			//或者是会员访问了游客缓存
			//旧网页继续使用
			cache.rate(ctx.old_obj);
		} else {
			cache.dead(ctx.old_obj, __FILE__, __LINE__);
		}
		return;
	}
	if (ctx.obj == NULL) {
		return;
	}
	//check can store
	if (ctx.obj->in_cache == 0) {
		stored_obj(this, ctx.obj, ctx.old_obj);
	} else {
		assert(ctx.obj->in_cache == 1);
		cache.rate(ctx.obj);
	}
}
const char* KHttpRequest::get_method() {
	return KHttpKeyValue::get_method(sink->data.meth)->data;
}
bool KHttpRequest::is_bad_url() {
	if (unlikely(sink->data.url == NULL || sink->data.url->is_bad() || sink->data.meth == METH_UNSET)) {
		return true;
	}
	return false;
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

KHttpRequest::~KHttpRequest() {
#ifdef ENABLE_REQUEST_QUEUE
	ReleaseQueue();
#endif
#if 0
	assert(sink);
	if (sink) {
		sink->end_request();
	}
#endif
}
KGL_RESULT KHttpRequest::sendfile(KASYNC_FILE fp, int64_t* total_length) {
	auto max_packet = conf.io_buffer;
	if (slh && max_packet > NBUFF_SIZE) {
		max_packet = NBUFF_SIZE;
	}
	while (*total_length > 0) {
		int length = (int)KGL_MIN(*total_length, max_packet);
		auto msec = get_sleep_msec(length);
		if (msec > 0) {
			kfiber_msleep(msec);
		}
		do {
			int got = sink->sendfile((kasync_file*)fp, length);
			if (got <= 0) {
				return KGL_EIO;
			}
			length -= got;
			*total_length -= got;
		} while (length > 0);
	}
	return KGL_OK;
}
/*
KGL_RESULT KHttpRequest::write_all(WSABUF* buf, int total_vc) {
	while (total_vc > 0) {
		int vc = total_vc;
		if (slh) {
			vc = 1;
			auto msec = get_sleep_msec(buf->iov_len);
			if (msec > 0) {
				kfiber_msleep(msec);
			}
		}
		do  {
			int got = sink->write(buf, vc);
			if (got <= 0) {
				return KGL_EIO;
			}
			while (got > 0) {
				if ((int)buf->iov_len > got) {
					buf->iov_len -= got;
					buf->iov_base = (char*)buf->iov_base + got;
					break;
				}
				assert(vc > 0);
				got -= (int)buf->iov_len;
				buf++;
				vc--;
				total_vc--;
				assert(got >= 0);
			}
		} while (vc > 0);
	}
	return KGL_OK;
}
*/
KGL_RESULT KHttpRequest::write_end(KGL_RESULT result) {
	assert(ctx.body.ctx);
	if (result == KGL_OK && sink->get_response_left() > 0) {
		//有content-length，又未读完
		result = KGL_ESOCKET_BROKEN;
	}
	ctx.body = { 0 };
	return result;
}
int KHttpRequest::write(const char* buf, int len) {
	auto msec = get_sleep_msec(len);
	if (msec > 0) {
		kfiber_msleep(msec);
	}
	return sink->write_all(buf, len);
}
int KHttpRequest::write(const kbuf* buf, int len) {
	if (!slh) {
		return sink->write_all(buf, len);
	}
	while (len>0) {
		int got = KGL_MIN(len, buf->used);
		auto msec = get_sleep_msec(got);
		if (msec > 0) {
			kfiber_msleep(msec);
		}
		len -= got;
		int left = sink->write_all(buf->data, got);
		if (left>0) {
			return len + left;
		}
		buf = buf->next;
	}
	return len;
}
int KHttpRequest::read(char* buf, int len) {
	return sink->read(buf, len);
}
KSubVirtualHost* KHttpRequest::get_virtual_host() {
	return static_cast<KSubVirtualHost*>(sink->data.opaque);
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
bool KHttpRequest::response_content_range(kgl_request_range* range, int64_t content_length)
{
	KStringBuf s;
	s.WSTR("bytes ");
	if (range) {
		s.add(range->from, INT64_FORMAT);
		s.WSTR("-");
		s.add(range->to, INT64_FORMAT);
		s.WSTR("/");
	} else {
		s.WSTR("*/");
	}	
	s.add(content_length, INT64_FORMAT);
	return response_header(kgl_header_content_range, s.buf(), s.size());
}
bool KHttpRequest::response_header(KHttpHeader* header, bool lock_header) {
	if (header->name_is_know) {
		assert(header->know_header < kgl_header_unknow);
		//use KGL_HEADER_VALUE_CONST will lock the header until startResponseBody called
		return sink->response_header((kgl_header_type)header->know_header, header->buf + header->val_offset, header->val_len, lock_header);
	}
	return sink->response_header(header->buf, header->name_len, header->buf + header->val_offset, header->val_len);
}
bool KHttpRequest::response_header(const char* name, hlen_t name_len, const char* val, hlen_t val_len) {
	return sink->response_header(name, name_len, val, val_len);
}
bool KHttpRequest::start_response_body(INT64 body_len) {
	return sink->start_response_body(body_len);
}

void KHttpRequest::insert_source(KFetchObject* fo) {
	fo->next = fo_head;
	if (!fo_last) {
		fo_last = fo;
	}
	fo_head = fo;
}
void KHttpRequest::append_source(KFetchObject* fo) {
	if (!fo->is_filter() && KBIT_TEST(ctx.filter_flags, RQ_NO_EXTEND) && !KBIT_TEST(sink->data.flags, RQ_IS_ERROR_PAGE)) {
		//无扩展处理
		if (fo->needQueue(this)) {
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
bool KHttpRequest::NeedQueue() {
	KFetchObject* fo = fo_head;
	while (fo) {
		if (fo->needQueue(this)) {
			return true;
		}
		fo = fo->next;
	}
	return false;
}
#ifdef ENABLE_TF_EXCHANGE
bool KHttpRequest::NeedTempFile(bool upload) {
	KFetchObject* fo = fo_head;
	while (fo) {
		if (fo->NeedTempFile(upload, this)) {
			return true;
		}
		fo = fo->next;
	}
	return false;
}
#endif
bool KHttpRequest::has_final_source() {
	if (!fo_last) {
		return false;
	}
	return !fo_last->is_filter();
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
