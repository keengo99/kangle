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
#include "lib.h"
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

using namespace std;
void WINAPI free_auto_memory(void *arg)
{
	xfree(arg);
}
void KHttpRequest::LeaveRequestQueue()
{
	setState(STATE_SEND);
	sink->RemoveSync();
}
void KHttpRequest::EnterRequestQueue()
{
	KBIT_SET(flags, RQ_QUEUED);
	setState(STATE_QUEUE);
	sink->AddSync();
}
void KHttpRequest::setState(uint8_t state) {
#ifdef ENABLE_STAT_STUB
	if (this->state == state) {
		return;
	}
	switch (this->state) {
	case STATE_IDLE:
	case STATE_QUEUE:
		katom_dec((void *)&kgl_waiting);
		break;
	case STATE_RECV:
		katom_dec((void *)&kgl_reading);
		break;
	case STATE_SEND:
		katom_dec((void *)&kgl_writing);
		break;
	}
#endif
	this->state = state;
#ifdef ENABLE_STAT_STUB
	switch (state) {
	case STATE_IDLE:
	case STATE_QUEUE:
		katom_inc((void *)&kgl_waiting);
		break;
	case STATE_RECV:
		katom_inc((void *)&kgl_reading);
		break;
	case STATE_SEND:
		katom_inc((void *)&kgl_writing);
		break;
	}
#endif		
}
void KHttpRequest::SetSelfPort(uint16_t port, bool ssl)
{
	if (port > 0) {
		self_port = port;
	}
	if (ssl) {
		KBIT_SET(raw_url.flags, KGL_URL_SSL);
		if (raw_url.port == 80) {
			raw_url.port = 443;
		}
	} else {
		KBIT_CLR(raw_url.flags, KGL_URL_SSL);
		if (raw_url.port == 443) {
			raw_url.port = 80;
		}
	}
}
const char *KHttpRequest::getState()
{
	
	switch (state) {
	case STATE_IDLE:
		return "idle";
	case STATE_SEND:
		return "send";
	case STATE_RECV:
		return "recv";
	case STATE_QUEUE:
		return "queue";
	}
	return "unknow";
	
}

void KHttpRequest::CloseFetchObject()
{
	for(;;) {
		kgl_list *pos = fo.next;
		if (pos == &fo) {
			break;
		}
		KFetchObject *fo = kgl_list_data(pos, KFetchObject, queue);
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
void KHttpRequest::set_url_param(char *param) {
	url->param = xstrdup(param);
}
int KHttpRequest::EndRequest() {
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	assert(bo_ctx == NULL);
#endif//}}
	ctx->clean_obj(this);
	log_access(this);
	return sink->EndRequest(this);
}
void KHttpRequest::beginRequest() {
#ifdef ENABLE_STAT_STUB
	katom_inc64((void *)&kgl_total_requests);
#endif
	setState(STATE_RECV);
	assert(url==NULL);
	mark = 0;
	url = new KUrl;
	if(raw_url.host){
		url->host = xstrdup(raw_url.host);
	}
	if (raw_url.param) {
		set_url_param(raw_url.param);
	}
	url->flag_encoding = raw_url.flag_encoding;
	url->port = raw_url.port;
	if(raw_url.path){
		url->path = xstrdup(raw_url.path);
		url_decode(url->path,0,url,false);
		KFileName::tripDir3(url->path, '/');
	}
	if (auth) {
		if (!auth->verifySession(this)) {
			delete auth;
			auth = NULL;
		}
	}
	if (KBIT_TEST(flags,RQ_INPUT_CHUNKED)) {
		left_read = -1;
	} else {
		left_read = content_length;
	}	
	begin_time_msec = kgl_current_msec;
#ifdef MALLOCDEBUG
	if (quit_program_flag!=PROGRAM_NO_QUIT) {
		KBIT_SET(flags,RQ_CONNECTION_CLOSE);
	}
#endif
}
const char *KHttpRequest::getMethod() {
	return KHttpKeyValue::getMethod(meth);
}
bool KHttpRequest::isBad() {
	if (unlikely(url==NULL || url->IsBad() || meth == METH_UNSET)) {
		return true;
	}
	return false;
}
KGL_RESULT KHttpRequest::HandleResult(KGL_RESULT result)
{
	if (result != KGL_OK) {
		KHttpObject* obj = ctx->obj;
		if (obj && obj->data->type == MEMORY_OBJECT) {
			obj->Dead();
		}
		KBIT_SET(flags, RQ_CONNECTION_CLOSE);
		//如果是http2的情况下，此处要向下游传递错误，调用terminal stream
		//避免下游会误缓存
		ctx->body_not_complete = 1;
	}
	return result;
}
void KHttpRequest::FreeLazyMemory() {
	if (client_ip) {
		free(client_ip);
		client_ip = NULL;
	}
	raw_url.destroy();
	free_header(header);
	header = last = NULL;
	mark = 0;
}
bool KHttpRequest::rewriteUrl(const char *newUrl, int errorCode, const char *prefix) {
	KUrl url2;
	if (!parse_url(newUrl, &url2)) {
		KStringBuf nu;
		if (prefix) {
			if (*prefix!='/') {
				nu << "/";
			}
			nu << prefix;
			int len = (int)strlen(prefix);
			if (len>0 && prefix[len-1]!='/') {
				nu << "/";
			}
			nu << newUrl;
		} else {
			char *basepath = strdup(url->path);
			char *p = strrchr(basepath,'/');
			if (p) {
				p++;
				*p = '\0';
			}
			nu << basepath;			
			free(basepath);		
			nu << newUrl;
		}	
		if(!parse_url(nu.getString(),&url2)){
			url2.destroy();
			return false;
		}
	}
	if (url2.path == NULL) {
		url2.destroy();
		return false;
	}
	if (KBIT_TEST(url2.flags, KGL_URL_ORIG_SSL)) {
		KBIT_SET(url2.flags, KGL_URL_SSL);
	}
	KStringBuf s;
	if (errorCode > 0) {
		s << errorCode << ";";
		if (KBIT_TEST(url->flags, KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		s << "://" << url->host	<< ":" << url->port << url->path;
		if (url->param) {
			s << "?" << url->param;
		}
	}
	if (url2.host == NULL) {
		url2.host = strdup(url->host);
	}
	url_decode(url2.path, 0, &url2);
	if (ctx->obj && ctx->obj->uk.url==url && !KBIT_TEST(ctx->obj->index.flags,FLAG_URL_FREE)) {
		KBIT_SET(ctx->obj->index.flags,FLAG_URL_FREE);
		ctx->obj->uk.url = url->clone();
	}
	url->destroy();
	url->host = url2.host;
	url->path = url2.path;
	if (errorCode > 0) {
		url->param = s.stealString();
		if (url2.param) {
			xfree(url2.param);
			url2.param = NULL;
		}
	} else {
		url->param = url2.param;
	}
	if (url2.port > 0) {
		url->port = url2.port;
	}
	if (url2.flags > 0) {
		url->flags = url2.flags;
	}
	KBIT_SET(raw_url.flags,KGL_URL_REWRITED);
	return true;
}
char *KHttpRequest::getUrl() {
	if (url==NULL) {
		return strdup("");
	}
	return url->getUrl();
}
std::string KHttpRequest::getInfo() {
	KStringBuf s;
//{{ent
#ifdef HTTP_PROXY
	if (meth==METH_CONNECT) {
		s << "CONNECT " ;
		if (raw_url.host) {
			s << raw_url.host << ":" << raw_url.port;
		}
		return s.getString();
	}
#endif//}}
	raw_url.GetUrl(s,true);
	return s.getString();
}
void KHttpRequest::init(kgl_pool_t *pool) {
	KHttpRequestData *data = static_cast<KHttpRequestData *>(this);
	memset(data, 0, sizeof(KHttpRequestData));
	InitPool(pool);
	begin_time_msec = kgl_current_msec;
	setState(STATE_IDLE);
}
KHttpRequest::~KHttpRequest()
{
	//printf("~KHttpRequest=[%p].\n", this);
	delete sink;
	clean();
	releaseVirtualHost();
	FreeLazyMemory();
	delete ctx;
#ifdef ENABLE_REQUEST_QUEUE
	assert(queue == NULL);
#endif
	setState(STATE_UNKNOW);
}
void KHttpRequest::clean() {
	CloseFetchObject();
	assert(ctx->queue_handled == 0);
	if (file){
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
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (bo_ctx) {
		delete bo_ctx;
		bo_ctx = NULL;
	}
#endif
	//}}
	if (url) {
		url->destroy();
		delete url;
		url = NULL;
	}
	while (slh) {
		KSpeedLimitHelper *slh_next = slh->next;
		delete slh;
		slh = slh_next;
	}
	if (bind_ip) {
		free(bind_ip);
		bind_ip = NULL;
	}
	while (fh) {
		KFlowInfoHelper *fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
	if (auth) {
		delete auth;
		auth = NULL;
	}
	if (pool) {
		kgl_destroy_pool(pool);
		pool = NULL;
	}
}
void KHttpRequest::releaseVirtualHost()
{
	if (svh) {
		svh->release();
		svh = NULL;
	}
}
bool KHttpRequest::parseMeth(const char *src) {
	meth = KHttpKeyValue::getMethod(src);
	if (meth >= 0) {
		return true;
	}
	return false;
}
bool KHttpRequest::parseConnectUrl(char *src) {
	char *ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	KBIT_CLR(raw_url.flags, KGL_URL_ORIG_SSL);
	*ss = 0;
	raw_url.host = strdup(src);
	raw_url.port = atoi(ss + 1);
	return true;
}
kgl_header_result KHttpRequest::parseHost(char *val)
{
	if (raw_url.host == NULL) {
		char *p = NULL ;
		if(*val == '['){
			KBIT_SET(raw_url.flags,KGL_URL_IPV6);
			val++;
			raw_url.host = strdup(val);
			p = strchr(raw_url.host,']');
			if(p){
				*p = '\0';
				p = strchr(p+1,':');
			}
		}else{
			raw_url.host = strdup(val);
			p = strchr(raw_url.host, ':');
			if(p){
				*p = '\0';
			}
		}
		if (p) {
			raw_url.port = atoi(p+1);
		} else {
			if (KBIT_TEST(raw_url.flags,KGL_URL_SSL)) {
				raw_url.port = 443;
			} else {
				raw_url.port = 80;
			}
		}
	}
	return kgl_header_no_insert;
}
bool KHttpRequest::WriteBuff(kbuf *buf)
{
	while (buf) {
		if (!WriteAll(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
int KHttpRequest::Write(WSABUF *buf, int bc)
{
	if (slh) {
		bc = 1;
		assert(!kfiber_is_main());
		int sleep_msec = getSleepTime(buf[0].iov_len);
		if (sleep_msec > 0) {
			kfiber_msleep(sleep_msec);
		}
	}
	int got = sink->Write(buf, bc);
	if (got > 0) {
		AddDownFlow(got);
	}
	return got;
}
int KHttpRequest::Write(const char *buf, int len)
{
	WSABUF bufs;
	bufs.iov_base = (char *)buf;
	bufs.iov_len = len;
	return Write(&bufs, 1);
}
bool KHttpRequest::WriteAll(const char *buf, int len)
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
void KHttpRequest::startParse() {
	FreeLazyMemory();
	if (KBIT_TEST(sink->GetBindServer()->flags, WORK_MODEL_SSL)) {
		KBIT_SET(raw_url.flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
	}
	meth = METH_UNSET;
}
kgl_header_result KHttpRequest::InternalParseHeader(const char *attr, int attr_len, char *val,int *val_len, bool is_first)
{
#ifdef ENABLE_HTTP2
	if (this->http_major>1 && *attr==':') {
		attr++;
		if (strcmp(attr, "method") == 0) {
			if (!parseMeth(val)) {
				klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
				return kgl_header_failed;
			}
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "version") == 0) {
			parseHttpVersion(val);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "path") == 0) {
			parse_url(val, &raw_url);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "authority") == 0) {
			if (kgl_header_success == parseHost(val)) {
				//转换成HTTP/1的http头
				AddHeader(kgl_expand_string("Host"), val, *val_len, true);
			}
			return kgl_header_no_insert;
		}
		return kgl_header_no_insert;
	}
#endif
	if (is_first && http_major<=1) {
		if (!parseMeth(attr)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return kgl_header_failed;
		}
		char *space = strchr(val, ' ');
		if (space == NULL) {
			klog(KLOG_DEBUG,"httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n",val);
			return kgl_header_failed;
		}
		*space = 0;
		space++;

		while (*space && IS_SPACE(*space)) {
			space++;
		}
		bool result;
		if (meth == METH_CONNECT) {
			result = parseConnectUrl(val);
		} else {
			result = parse_url(val, &raw_url);
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return kgl_header_failed;
		}
		if (!parseHttpVersion(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return kgl_header_failed;
		}
		if (http_major > 1 || (http_major == 1 && http_minor == 1)) {
			KBIT_SET(flags, RQ_HAS_KEEP_CONNECTION);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Host")) {
		return parseHost(val);
	}
	if (!strcasecmp(attr, "Connection")
		//{{ent
#ifdef HTTP_PROXY
		|| !strcasecmp(attr, "proxy-connection")
#endif//}}
		) {
		KHttpFieldValue field(val);
		do {
			if (field.is2("keep-alive", 10)) {
				flags |= RQ_HAS_KEEP_CONNECTION;
			} else if (field.is2("upgrade", 7)) {
				flags |= RQ_HAS_CONNECTION_UPGRADE;
			} else if (field.is2(kgl_expand_string("close"))) {
				KBIT_CLR(flags, RQ_HAS_KEEP_CONNECTION);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		if (!*val) {
			return kgl_header_no_insert;
		}
		KHttpFieldValue field(val);
		do {
			if (field.is2(kgl_expand_string("gzip"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_GZIP);
			} else if (field.is2(kgl_expand_string("deflate"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_DEFLATE);
			} else if (field.is2(kgl_expand_string("compress"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_COMPRESS);
			} else if (field.is2(kgl_expand_string("br"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_BR);
			} else if (!field.is2(kgl_expand_string("identity"))) {
				KBIT_SET(raw_url.accept_encoding, KGL_ENCODING_UNKNOW);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "If-Range")) {
		time_t try_time = parse1123time(val);
		if (try_time == -1) {
			flags |= RQ_IF_RANGE_ETAG;
			if (this->ctx->if_none_match == NULL) {
				ctx->set_if_none_match(val, *val_len);
			}
		} else {
			if_modified_since = try_time;
			flags |= RQ_IF_RANGE_DATE;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		if_modified_since = parse1123time(val);
		flags |= RQ_HAS_IF_MOD_SINCE;
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-None-Match")) {
		flags |= RQ_HAS_IF_NONE_MATCH;
		if (this->ctx->if_none_match == NULL) {
			ctx->set_if_none_match(val, *val_len);
		}
		return kgl_header_no_insert;
	}
	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		content_length = string2int(val);
		left_read = content_length;
		flags |= RQ_HAS_CONTENT_LEN;
		return kgl_header_no_insert;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			KBIT_SET(flags, RQ_INPUT_CHUNKED);
			content_length = -1;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			flags |= RQ_HAVE_EXPECT;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "X-Forwarded-Proto")) {
		if (strcasecmp(val, "https") == 0) {
			KBIT_SET(raw_url.flags, KGL_URL_ORIG_SSL);
		} else {
			KBIT_CLR(raw_url.flags, KGL_URL_ORIG_SSL);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			flags |= RQ_HAS_NO_CACHE;
		return kgl_header_success;
	}
	if (
		//{{ent
#ifdef HTTP_PROXY
		(KBIT_TEST(GetWorkModel(),WORK_MODEL_MANAGE) && !strcasecmp(attr, "Authorization")) ||
#endif//}}
		!strcasecmp(attr, AUTH_REQUEST_HEADER)) {
		//{{ent
#ifdef HTTP_PROXY
		flags |= RQ_HAS_PROXY_AUTHORIZATION;
#else//}}
		flags |= RQ_HAS_AUTHORIZATION;
		//{{ent
#endif//}}
#ifdef ENABLE_TPROXY
		if (!KBIT_TEST(GetWorkModel(),WORK_MODEL_TPROXY)) {
#endif
			char *p = val;
			while (*p && !IS_SPACE(*p)) {
				p++;
			}
			char *p2 = p;
			while (*p2 && IS_SPACE(*p2)) {
				p2++;
			}
			KHttpAuth *tauth = NULL;
			if (strncasecmp(val, "basic", p - val) == 0) {
				tauth = new KHttpBasicAuth;
			} else if (strncasecmp(val, "digest", p - val) == 0) {
#ifdef ENABLE_DIGEST_AUTH
				tauth = new KHttpDigestAuth;
#endif
			}
			if (tauth) {
				if (!tauth->parse(this, p2)) {
					delete tauth;
					tauth = NULL;
				}
			}
			if (auth) {
				delete auth;
			}
			auth = tauth;
			//{{ent
#ifdef HTTP_PROXY
			return kgl_header_no_insert;
#endif//}}
#ifdef ENABLE_TPROXY
		}
#endif
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-store") || field.is("no-cache")) {
				flags |= RQ_HAS_NO_CACHE;
			} else if (field.is("only-if-cached")) {
				flags |= RQ_HAS_ONLY_IF_CACHED;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Range")) {
		if (!strncasecmp(val, "bytes=", 6)) {
			val += 6;
			range_from = -1;
			range_to = -1;
			if (*val != '-') {
				range_from = string2int(val);
			}
			char *p = strchr(val, '-');
			if (p && *(p + 1)) {
				range_to = string2int(p + 1);
			}
			char *next_range = strchr(val, ',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO, "cut multi_range %s\n", val);
				//KBIT_SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		flags |= RQ_HAVE_RANGE;
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Content-Type")) {
#ifdef ENABLE_INPUT_FILTER
		if (if_ctx == NULL) {
			if_ctx = new KInputFilterContext(this);
		}
#endif
		if (strncasecmp(val, "multipart/form-data", 19) == 0) {
			KBIT_SET(flags, RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			if_ctx->parseBoundary(val + 19);
#endif
		}
		return kgl_header_success;
	}
	return kgl_header_success;
}
int KHttpRequest::Read(char *buf, int len)
{
	kassert(!kfiber_is_main());
	if (KBIT_TEST(flags, RQ_HAVE_EXPECT)) {
		KBIT_CLR(flags, RQ_HAVE_EXPECT);
		sink->ResponseStatus(this, 100);
		sink->StartResponseBody(this, 0);
		sink->Flush();
		sink->StartHeader(this);
	}
	int length;
	if (left_read >= 0 && !ctx->connection_upgrade) {
		len = (int)MIN((int64_t)len, left_read);
	}
	length = sink->Read(buf,len);
	if (length == 0 && left_read == -1 && !ctx->connection_upgrade) {
		left_read = 0;
		return 0;
	}
	if (length > 0) {
		if (!ctx->connection_upgrade && left_read != -1) {
			left_read -= length;
		}
		AddUpFlow((INT64)length);
	}
	return length;
}

bool KHttpRequest::ParseHeader(const char *attr, int attr_len,char *val,int val_len, bool is_first)
{
	if (is_first) {
		startParse();
	}
	//printf("attr=[%s] val=[%s]\n", attr, val);
	kgl_header_result ret = InternalParseHeader(attr, attr_len, val,&val_len, is_first);
	switch (ret) {
	case kgl_header_failed:
		return false;
	case kgl_header_insert_begin:
		return AddHeader(attr, attr_len, val, val_len, false);
	case kgl_header_success:
		return AddHeader(attr, attr_len, val, val_len, true);
	default:
		return true;
	}
}
bool KHttpRequest::has_post_data() {
	return left_read>0;
}
bool KHttpRequest::parseHttpVersion(char *ver) {
	char *dot = strchr(ver,'.');
	if (dot==NULL) {
		return false;
	}
	http_major = *(dot - 1) - 0x30;//major;
	http_minor = *(dot + 1) - 0x30;//minor;
	return true;
}

int KHttpRequest::checkFilter(KHttpObject *obj) {
	int action = JUMP_ALLOW;
	if (KBIT_TEST(obj->index.flags,FLAG_NO_BODY)) {
		return action;
	}
	if (of_ctx) {
		if(of_ctx->charset == NULL){
			of_ctx->charset = obj->getCharset();
		}
		kbuf *bodys = obj->data->bodys;
		kbuf *head = bodys;
		while (head && head->used > 0) {
			action = of_ctx->checkFilter(this,head->data, head->used);
			if (action == JUMP_DENY) {
				break;
			}
			head = head->next;
		}
	}
	return action;
}
void KHttpRequest::addFilter(KFilterHelper *chain) {
	getOutputFilterContext()->addFilter(chain);
}
KOutputFilterContext *KHttpRequest::getOutputFilterContext()
{
	if (of_ctx==NULL) {
		of_ctx = new KOutputFilterContext;
	}
	return of_ctx;
}
void KHttpRequest::ResponseVary(const char *vary)
{
	KHttpField field;
	field.parse(vary, ',');
	KStringBuf s;
	http_field_t *head = field.getHeader();
	while (head) {		
		if (*head->attr != KGL_VARY_EXTEND_CHAR) {
			if (s.getSize() > 0) {
				s.write_all(kgl_expand_string(", "));
			}
			s << head->attr;
		} else {
			//vary extend
			char *name = head->attr + 1;
			char *value = strchr(name, '=');
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
		responseHeader(kgl_expand_string("Vary"), s.getString(), len);
	}
}
char *KHttpRequest::BuildVary(const char *vary)
{
	KHttpField field;
	field.parse(vary, ',');
	KStringBuf s;
	http_field_t *head = field.getHeader();
	while (head) {
		if (strcasecmp(head->attr, "Accept-Encoding") != 0) {
			if (*head->attr == KGL_VARY_EXTEND_CHAR) {
				//vary extend
				char *name = head->attr + 1;
				char *value = strchr(name, '=');
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
				KHttpHeader *rq_header = this->FindHeader(head->attr, strlen(head->attr));
				if (rq_header) {
					s.write_all(rq_header->val, rq_header->val_len);
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

bool KHttpRequest::responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len)
{
	return sink->ResponseHeader(name, name_len, val, val_len);
}
bool KHttpRequest::responseContentLength(int64_t content_len)
{
	if (content_len >= 0) {
		//有content-length时
		char len_str[INT2STRING_LEN];
		int len = int2string2(content_len, len_str, false);
		return responseHeader(kgl_expand_string("Content-Length"), len_str, len);
	}
	//无content-length时
	if (http_minor == 0) {
		//A HTTP/1.0 client no support TE head.
		//The connection MUST close
		KBIT_SET(flags, RQ_CONNECTION_CLOSE);
	} else if (!ctx->connection_upgrade	&& sink->SetTransferChunked()) {
		KBIT_SET(flags, RQ_TE_CHUNKED);
	}
	return true;	
}
bool KHttpRequest::startResponseBody(INT64 body_len)
{
	assert(!KBIT_TEST(flags,RQ_HAS_SEND_HEADER));
	if (KBIT_TEST(flags,RQ_HAS_SEND_HEADER)) {
		return true;
	}
	KBIT_SET(flags,RQ_HAS_SEND_HEADER);
	if (meth == METH_HEAD) {
		body_len = 0;
	}
	int header_len = sink->StartResponseBody(this, body_len);
	AddDownFlow(header_len,true);
	return header_len > 0;
}

void KHttpRequest::InsertFetchObject(KFetchObject *fo)
{
	if (fo->need_check) {
		ctx->fo_need_check = 1;
	}
	klist_append(this->fo.prev, &fo->queue);
}
void KHttpRequest::AppendFetchObject(KFetchObject *fo)
{
	if (fo->filter==0 && KBIT_TEST(filter_flags, RQ_NO_EXTEND) && !KBIT_TEST(flags, RQ_IS_ERROR_PAGE)) {
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
	kgl_list *pos = this->fo.prev;
	if (pos == &this->fo) {
		return false;
	}
	KFetchObject *fo = kgl_list_data(pos, KFetchObject, queue);
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
