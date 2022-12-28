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
#include "KFilterHelper.h"
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
#include "KHttpFilterManage.h"
#include "KHttpFilterContext.h"
#include "KBufferFetchObject.h"
 //#include "KHttpTransfer.h"
#include "KVirtualHostManage.h"
#include "kselectable.h"
#include "KStaticFetchObject.h"
#include "KAccessDsoSupport.h"
#include "kfiber.h"
#include "KSockPoolHelper.h"

using namespace std;

void WINAPI free_auto_memory(void* arg)
{
	xfree(arg);
}
char* KHttpRequest::map_url_path(const char* url, KBaseRedirect* caller)
{
	KSubVirtualHost* nsvh = NULL;
	auto svh = get_virtual_host();
	if (svh == NULL) {
		return NULL;
	}
	char* filename = NULL;
	KAutoUrl u;
	const char* path = url;
	if (parse_url(url, u.u) && u.u->host != nullptr) {
		path = u.u->path;
		conf.gvm->queryVirtualHost((KVirtualHostContainer*)sink->get_server_opaque(), &nsvh, u.u->host, 0);
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
void KHttpRequest::LeaveRequestQueue()
{
	sink->set_state(STATE_SEND);
}
void KHttpRequest::EnterRequestQueue()
{
	KBIT_SET(sink->data.flags, RQ_QUEUED);
	sink->set_state(STATE_QUEUE);
}
void KHttpRequest::SetSelfPort(uint16_t port, bool ssl)
{
	sink->set_self_port(port, ssl);
}
void KHttpRequest::CloseFetchObject()
{
	for (;;) {
		kgl_list* pos = fo.next;
		if (pos == &fo) {
			break;
		}
		KFetchObject* fo = kgl_list_data(pos, KFetchObject, queue);
		klist_remove(pos);
		delete fo;
	}
#ifdef ENABLE_REQUEST_QUEUE
	ReleaseQueue();
#endif
}
#ifdef ENABLE_REQUEST_QUEUE
void KHttpRequest::ReleaseQueue()
{
	if (queue) {
		if (ctx->queue_handled) {
			queue->Unlock();
			ctx->queue_handled = 0;
		}
		queue->release();
		queue = NULL;
	}
}
#endif
void KHttpRequest::set_url_param(char* param) {
	assert(sink->data.url->param == NULL);
	sink->data.url->param = xstrdup(param);
}
KHttpHeaderIteratorResult handle_http_header(void* arg, KHttpHeader* header)
{
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
	if (kgl_is_attr(header, _KS("Content-Type"))) {
#ifdef ENABLE_INPUT_FILTER
		if (rq->if_ctx == NULL) {
			rq->if_ctx = new KInputFilterContext(rq);
		}
#endif
		if (kgl_ncasecmp(header->buf + header->val_offset, header->val_len, _KS("multipart/form-data")) == 0) {
			KBIT_SET(rq->sink->data.flags, RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			rq->if_ctx->parse_boundary(header->buf + header->val_offset + 19, header->val_len - 19);
#endif
		}
	}
	return KHttpHeaderIteratorResult::Continue;
}
void KHttpRequest::beginRequest()
{
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
uint32_t KHttpRequest::get_upstream_flags()
{
	uint32_t flags = 0;
	if (KBIT_TEST(sink->data.flags, RQ_UPSTREAM_ERROR)) {
		KBIT_SET(flags, KSOCKET_FLAGS_SKIP_POOL);
	}
	if (KBIT_TEST(sink->data.flags, RQ_HAS_CONNECTION_UPGRADE)) {
		KBIT_SET(flags, KSOCKET_FLAGS_SKIP_POOL | KSOCKET_FLAGS_WEBSOCKET);
	}
	return flags;
}
int KHttpRequest::EndRequest() {
#ifdef ENABLE_BIG_OBJECT_206
	assert(bo_ctx == NULL);
#endif
	ctx->clean_obj(this);
	log_access(this);
	delete this;
	return 0;
}
const char* KHttpRequest::get_method() {
	return KHttpKeyValue::get_method(sink->data.meth)->data;
}
bool KHttpRequest::isBad() {
	if (unlikely(sink->data.url == NULL || sink->data.url->IsBad() || sink->data.meth == METH_UNSET)) {
		return true;
	}
	return false;
}
bool KHttpRequest::rewriteUrl(const char* newUrl, int errorCode, const char* prefix) {
	KAutoUrl url2;
	if (!parse_url(newUrl, url2.u)) {
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
		if (!parse_url(nu.getString(), url2.u)) {
			return false;
		}
	}
	if (url2.u->path == NULL) {
		return false;
	}
	if (KBIT_TEST(url2.u->flags, KGL_URL_ORIG_SSL)) {
		KBIT_SET(url2.u->flags, KGL_URL_SSL);
	}
	KStringBuf s;
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
	if (url2.u->host == NULL) {
		url2.u->host = strdup(sink->data.url->host);
	}
	url_decode(url2.u->path, 0, url2.u);
	if (ctx->obj && ctx->obj->uk.url == sink->data.url) {// && !KBIT_TEST(ctx->obj->index.flags,FLAG_URL_FREE)) {
		ctx->obj->uk.url->relase();
		ctx->obj->uk.url = url2.u->refs();
	}
	sink->data.url->relase();
	sink->data.url = url2.u->refs();

	if (errorCode > 0) {
		if (sink->data.url->param) {
			xfree(sink->data.url->param);
		}
		sink->data.url->param = s.stealString();
	}
	KBIT_SET(sink->data.raw_url->flags, KGL_URL_REWRITED);
	return true;
}
char* KHttpRequest::getUrl() {
	if (sink->data.url == NULL) {
		return strdup("");
	}
	return sink->data.url->getUrl();
}
std::string KHttpRequest::getInfo() {
	KStringBuf s;
#ifdef HTTP_PROXY
	if (meth == METH_CONNECT) {
		s << "CONNECT ";
		if (sink->data.raw_url->host) {
			s << sink->data.raw_url->host << ":" << sink->data.raw_url->port;
		}
		return s.getString();
	}
#endif
	sink->data.raw_url->GetUrl(s, true);
	return s.getString();
}

KHttpRequest::~KHttpRequest()
{
	CloseFetchObject();
	assert(ctx->queue_handled == 0);
	if (file) {
		delete file;
		file = NULL;
	}
	assert(ctx);
	ctx->clean();
	//tr由ctx->st负责删除
	tr = NULL;
	if (of_ctx) {
		delete of_ctx;
		of_ctx = NULL;
	}
#ifdef ENABLE_INPUT_FILTER
	if (if_ctx) {
		delete if_ctx;
		if_ctx = NULL;
	}
#endif
#ifdef ENABLE_BIG_OBJECT_206
	if (bo_ctx) {
		delete bo_ctx;
		bo_ctx = NULL;
	}
#endif
	while (slh) {
		KSpeedLimitHelper* slh_next = slh->next;
		delete slh;
		slh = slh_next;
	}
	if (auth) {
		delete auth;
		auth = NULL;
	}
	delete ctx;
#ifdef ENABLE_REQUEST_QUEUE
	assert(queue == NULL);
#endif
	assert(sink);
	if (sink) {
		sink->end_request();
	}
}
bool KHttpRequest::WriteBuff(kbuf* buf)
{
	while (buf) {
		if (!WriteAll(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
int KHttpRequest::Write(WSABUF* buf, int bc)
{
	if (slh) {
		bc = 1;
	}
	int got = sink->write(buf, bc);
	if (got > 0) {
		assert(!kfiber_is_main());
		int sleep_msec = getSleepTime(got);
		if (sleep_msec > 0) {
			kfiber_msleep(sleep_msec);
		}
	}
	return got;
}
bool KHttpRequest::write_all(WSABUF* buf, int *vc)
{
	while (*vc > 0) {
		int got = sink->write(buf, slh?1:*vc);
		if (got <= 0) {
			return false;
		}
		int sleep_msec = getSleepTime(got);
		if (sleep_msec > 0) {
			kfiber_msleep(sleep_msec);
		}
		while (got > 0) {
			if ((int)buf->iov_len > got) {
				buf->iov_len += got;
				buf->iov_base = (char*)buf->iov_base + got;
				break;
			}
			got -= (int)buf->iov_len;
			buf += 1;
			*vc -= 1;
		}
	}
	return true;
}
int KHttpRequest::Write(const char* buf, int len)
{
	WSABUF bufs;
	bufs.iov_base = (char*)buf;
	bufs.iov_len = len;
	return Write(&bufs, 1);
}
bool KHttpRequest::WriteAll(const char* buf, int len)
{
	while (len > 0) {
		int this_len = Write(buf, len);
		if (this_len <= 0) {
			return false;
		}
		len -= this_len;
		buf += this_len;
	}
	return true;
}
int KHttpRequest::Read(char* buf, int len)
{
	return sink->read(buf, len);
}
KSubVirtualHost* KHttpRequest::get_virtual_host()
{
	return static_cast<KSubVirtualHost*>(sink->data.opaque);
}

bool KHttpRequest::has_post_data(kgl_input_stream* in) {
	return in->f->get_read_left(in, this) != 0;
}
int KHttpRequest::checkFilter(KHttpObject* obj) {
	int action = JUMP_ALLOW;
	if (KBIT_TEST(obj->index.flags, FLAG_NO_BODY)) {
		return action;
	}
	if (of_ctx) {
		if (of_ctx->charset == NULL) {
			of_ctx->charset = obj->getCharset();
		}
		kbuf* bodys = obj->data->bodys;
		kbuf* head = bodys;
		while (head && head->used > 0) {
			action = of_ctx->checkFilter(this, head->data, head->used);
			if (action == JUMP_DENY) {
				break;
			}
			head = head->next;
		}
	}
	return action;
}

void KHttpRequest::addFilter(KFilterHelper* chain) {
	getOutputFilterContext()->addFilter(chain);
}
KOutputFilterContext* KHttpRequest::getOutputFilterContext()
{
	if (of_ctx == NULL) {
		of_ctx = new KOutputFilterContext;
	}
	return of_ctx;
}
void KHttpRequest::ResponseVary(const char* vary)
{
	KHttpField field;
	field.parse(vary, ',');
	KStringBuf s;
	http_field_t* head = field.getHeader();
	while (head) {
		if (*head->attr != KGL_VARY_EXTEND_CHAR) {
			if (s.getSize() > 0) {
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
	int len = s.getSize();
	if (len > 0) {
		response_header(kgl_expand_string("Vary"), s.getString(), len);
	}
}
char* KHttpRequest::BuildVary(const char* vary)
{
	KHttpField field;
	field.parse(vary, ',');
	KStringBuf s;
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
					KBIT_SET(this->filter_flags, RF_NO_CACHE);
					return NULL;
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
	if (s.getSize() == 0) {
		return NULL;
	}
	return s.stealString();
}
bool KHttpRequest::response_header(KHttpHeader* header,bool lock_header)
{
	if (header->name_is_know) {
		assert(header->know_header < kgl_header_unknow);
		//use KGL_HEADER_VALUE_CONST will lock the header until startResponseBody called
		return sink->response_header((kgl_header_type)header->know_header, header->buf+ header->val_offset, header->val_len, lock_header);
	}
	return sink->response_header(header->buf, header->name_len, header->buf+header->val_offset, header->val_len);	
}
bool KHttpRequest::response_header(const char* name, hlen_t name_len, const char* val, hlen_t val_len)
{
	return sink->response_header(name, name_len, val, val_len);
}
bool KHttpRequest::start_response_body(INT64 body_len)
{
	return sink->start_response_body(body_len);
}

void KHttpRequest::InsertFetchObject(KFetchObject* fo)
{
	if (fo->need_check) {
		ctx->fo_need_check = 1;
	}
	klist_append(this->fo.prev, &fo->queue);
}
void KHttpRequest::AppendFetchObject(KFetchObject* fo)
{
	if (fo->filter == 0 && KBIT_TEST(filter_flags, RQ_NO_EXTEND) && !KBIT_TEST(sink->data.flags, RQ_IS_ERROR_PAGE)) {
		//无扩展处理
		if (fo->needQueue(this)) {
			delete fo;
			fo = new KStaticFetchObject();
		}
		if (fo->need_check) {
			ctx->fo_need_check = 1;
		}
	}
	klist_append(&this->fo, &fo->queue);
}
bool KHttpRequest::NeedQueue()
{
	kgl_list* pos;
	klist_foreach(pos, &fo) {
		KFetchObject* fo = kgl_list_data(pos, KFetchObject, queue);
		if (fo->needQueue(this)) {
			return true;
		}
	}
	return false;
}
bool KHttpRequest::NeedTempFile(bool upload)
{
	kgl_list* pos;
	klist_foreach(pos, &fo) {
		KFetchObject* fo = kgl_list_data(pos, KFetchObject, queue);
		if (fo->NeedTempFile(upload, this)) {
			return true;
		}
	}
	return false;
}
bool KHttpRequest::NeedCheck()
{
	if (!ctx->fo_need_check) {
		return false;
	}
	kgl_list* pos = this->fo.prev;
	if (pos == &this->fo) {
		return false;
	}
	return true;
}
bool KHttpRequest::HasFinalFetchObject()
{
	kgl_list* pos = this->fo.prev;
	if (pos == &this->fo) {
		return false;
	}
	KFetchObject* fo = kgl_list_data(pos, KFetchObject, queue);
	return !fo->filter;
}

KGL_RESULT KHttpRequest::OpenNextFetchObject(KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, const char* queue)
{
	KFetchObject* next = this->GetNextFetchObject(fo);
	if (next == NULL) {
		return KGL_ENOT_READY;
	}
#ifdef ENABLE_REQUEST_QUEUE
	if (queue) {
		ReleaseQueue();
		this->queue = get_request_queue(this, queue);
		return open_queued_fetchobj(this, next, in, out, this->queue);
	}
#endif
	return open_fetchobj(this, next, in, out);
}
