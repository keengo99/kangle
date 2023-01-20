#include "KHttpRequest.h"
#include "KAccessDsoSupport.h"
#include "http.h"
#include "ssl_utils.h"
#include "KDsoRedirect.h"
#include "KFilterContext.h"
#include "KDsoFilter.h"
#ifdef ENABLE_BLACK_LIST
#include "KWhiteList.h"
#endif
static KMutex queue_lock;
static std::map<char*, KRequestQueue*, lessp> queues;

#define ADD_VAR(x,y,z) add_api_var(x,y,z,sizeof(z)-1)
KGL_RESULT add_api_var(LPVOID buffer, LPDWORD size, const char* val, int len)
{
	if (len == 0) {
		len = (int)strlen(val);
	}
	if ((int)*size <= len) {
		*size = len + 1;
		return KGL_EINSUFFICIENT_BUFFER;
	}
	kgl_memcpy(buffer, val, len + 1);
	*size = len;
	return KGL_OK;
}
KGL_RESULT var_printf(LPVOID buffer, LPDWORD size, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf((char*)buffer, *size, fmt, ap);
	va_end(ap);
	if (len < (int)*size) {
		*size = len;
		return KGL_OK;
	}
	*size = len + 1;
	return KGL_EINSUFFICIENT_BUFFER;
}
KGL_RESULT add_header_var(LPVOID buffer, LPDWORD size, KHttpHeader* header, const char* name, size_t len)
{
	while (header) {
		if (kgl_is_attr(header, name, len)) {
			return add_api_var(buffer, size, header->buf + header->val_offset, header->val_len);
		}
		header = header->next;
	}
	return KGL_ENO_DATA;
}
KGL_RESULT get_response_variable(KHttpRequest* rq, KGL_VAR type, const char *name, LPVOID  buffer, LPDWORD size)
{
	KHttpObject* obj = rq->ctx.obj;
	if (obj == NULL) {
		return KGL_ENO_DATA;
	}
	switch (type) {
	case KGL_VAR_HEADER:
		return add_header_var(buffer, size, obj->data->headers, name, strlen(name));
	default:
		return KGL_ENOT_SUPPORT;
	}
}
KGL_RESULT get_request_variable(KHttpRequest* rq, KGL_VAR type, const char *name, LPVOID  buffer, LPDWORD size)
{
	switch (type) {
	case KGL_VAR_HEADER:
		return add_header_var(buffer, size, rq->sink->data.get_header(), name, strlen(name));
#ifdef KSOCKET_SSL
	case KGL_VAR_SSL_VAR:
	{
		kssl_session* ssl = rq->sink->get_ssl();
		if (ssl) {
			char* result = ssl_var_lookup(ssl->ssl, name);
			if (result) {
				KGL_RESULT ret = add_api_var(buffer, size, result);
				OPENSSL_free(result);
				return ret;
			}
		}
		return KGL_EUNKNOW;
	}
#endif
	case KGL_VAR_HTTPS:
	{
		int* v = (int*)buffer;
		if (KBIT_TEST(rq->sink->data.url->flags, KGL_URL_SSL)) {
			*v = 1;
		} else {
			*v = 0;
		}
		return KGL_OK;
	}
	case KGL_VAR_CACHE_TYPE:
	{
		int32_t* v = (int32_t*)buffer;
		if (rq->ctx.obj) {
			if (rq->ctx.obj->in_cache) {
				*v = 1;
				return KGL_OK;
			}
			if (rq->ctx.old_obj) {
				*v = 2;
				return KGL_OK;
			}
		}
		*v = 0;
		return KGL_OK;
	}
	case KGL_VAR_SERVER_PROTOCOL:
		if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP)) {
			return ADD_VAR(buffer, size, "TCP");
		}
		switch (rq->sink->data.http_version) {
		case 0x200:
			return ADD_VAR(buffer, size, "HTTP/2");
		case 0x300:
			return ADD_VAR(buffer, size, "HTTP/3");
		case 0x100:
			return ADD_VAR(buffer, size, "HTTP/1.0");
		default:
			return ADD_VAR(buffer, size, "HTTP/1.1");
		}
	case KGL_VAR_SERVER_NAME:
		return add_api_var(buffer, size, rq->sink->data.url->host);
	case KGL_VAR_REQUEST_METHOD:
		return add_api_var(buffer, size, rq->get_method());
	case KGL_VAR_PATH_INFO:
		return add_api_var(buffer, size, rq->sink->data.url->path);
	case KGL_VAR_URL:
	{
		KStringBuf s;
		rq->sink->data.url->GetUrl(s);
		return add_api_var(buffer, size, s.getString(), s.getSize());
	}
	case KGL_VAR_REQUEST_URI:
		if (rq->sink->data.raw_url->param == NULL) {
			return add_api_var(buffer, size, rq->sink->data.raw_url->path);
		}
		return var_printf(buffer, size, "%s?%s", rq->sink->data.raw_url->path, rq->sink->data.raw_url->param);
	case KGL_VAR_SCRIPT_NAME:
		return add_api_var(buffer, size, rq->sink->data.url->path);
	case KGL_VAR_QUERY_STRING:
	{
		const char* param = rq->sink->data.url->param;
		KGL_RESULT ret = KGL_ENO_DATA;
		if (param) {
			ret = add_api_var(buffer, size, param);
		}
		return ret;
	}
	case KGL_VAR_SERVER_ADDR:
	{
		if (rq->sink->get_self_ip((char*)buffer, *size - 1) == 0) {
			*size = MAXIPLEN;
			return KGL_EINSUFFICIENT_BUFFER;
		}
		return KGL_OK;
	}
	case KGL_VAR_SERVER_PORT:
	{
		uint16_t* v = (uint16_t*)buffer;
		*v = rq->sink->data.raw_url->port;
		return KGL_OK;
	}
	case KGL_VAR_REMOTE_ADDR:
	{
		return add_api_var(buffer, size, rq->getClientIp());
	}
	case KGL_VAR_REMOTE_PORT:
	{
		uint16_t* v = (uint16_t*)buffer;
		*v = ksocket_addr_port(rq->sink->get_peer_addr());
		return KGL_OK;
	}
	case KGL_VAR_PEER_ADDR:
	{
		if (rq->sink->get_peer_ip((char*)buffer, *size - 1)) {
			*size = (int)strlen((char*)buffer);
			return KGL_OK;
		}
		*size = MAXIPLEN;
		return KGL_EINSUFFICIENT_BUFFER;
	}
	case KGL_VAR_DOCUMENT_ROOT:
	{
		auto svh = rq->get_virtual_host();
		if (svh == NULL) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, svh->doc_root);
	}
	case KGL_VAR_HAS_CONNECTION_UPGRADE:
	{
		bool* v = (bool*)buffer;
		*v = KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONNECTION_UPGRADE) > 0;
		return KGL_OK;
	}
	case KGL_VAR_HAS_CONTENT_LENGTH:
	{
		bool* v = (bool*)buffer;
		*v = KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONTENT_LEN) > 0;
		return KGL_OK;
	}
	case KGL_VAR_CONTENT_TYPE:
		return add_header_var(buffer, size, rq->sink->data.get_header(), _KS("Content-Type"));
	default:
		return KGL_ENOT_SUPPORT;
	}
	/*
	if (strcasecmp(name, "AUTH_USER") == 0) {
		if (rq->auth) {
			return add_api_var(buffer, size, rq->auth->getUser());
		}
		return KGL_ENO_DATA;
	}
	*/
}
static KGL_RESULT  set_request_header(
	KREQUEST r,
	KCONN cn,
	const char* attr,
	hlen_t attr_len,
	const char* val,
	hlen_t val_len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	bool result = rq->sink->parse_header(attr, attr_len, val, val_len, false);
	if (!result) {
		return KGL_EINVALID_PARAMETER;
	}
	return KGL_OK;
}

static KGL_RESULT  add_obj_unknow_header(KREQUEST r,
	KCONN cn,
	const char* attr,
	hlen_t attr_len,
	const char* val,
	hlen_t val_len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	KHttpResponseParser parser;
	if (!parser.parse_unknow_header(rq, attr, attr_len, val, val_len, false)) {
		return KGL_EINVALID_PARAMETER;
	}
	parser.commit_headers(rq);
	return KGL_OK;
}

static KGL_RESULT  response_unknow_header(
	KREQUEST r,
	KCONN cn,
	const char* attr,
	hlen_t attr_len,
	const char* val,
	hlen_t val_len)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	if (kgl_mem_case_same(attr, attr_len, _KS("Status"))) {
		rq->response_status(kgl_atoi((u_char*)val, val_len));
		return KGL_OK;
	}
	rq->response_header(attr, attr_len, val, val_len);
	return KGL_OK;
}
#if 0
static KGL_RESULT  request_write_client(
	KREQUEST r,
	KCONN cn,
	LPVOID                        Buffer,
	LPDWORD                       lpdwBytes
)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	KAccessContext* ar = (KAccessContext*)cn;
	int len = ar->GetBuffer(rq)->write(rq, (char*)Buffer, *lpdwBytes);
	if (len <= 0) {
		return KGL_EUNKNOW;
	}
	*lpdwBytes = len;
	return KGL_OK;
}
static KGL_RESULT  response_write_client(
	KCONN cn,
	LPVOID                        Buffer,
	LPDWORD                       lpdwBytes
) {
	return KGL_ENOT_SUPPORT;
}
#endif
KGL_RESULT base_support_function(KHttpRequest* rq, KF_REQ_TYPE req, PVOID data, PVOID* ret)
{
	switch (req) {
	case KD_REQ_REWRITE_PARAM:
	{
		const char* param = (const char*)data;
		if (rq->sink->data.url->param) {
			xfree(rq->sink->data.url->param);
			rq->sink->data.url->param = NULL;
		}
		if (param && *param) {
			rq->sink->data.url->param = strdup(param);
		}
		KBIT_SET(rq->sink->data.raw_url->flags, KGL_URL_REWRITED);
		return KGL_OK;
	}
	case KD_REQ_REWRITE_URL:
	{
		if (rq->rewrite_url((const char*)data)) {
			return KGL_OK;
		}
		return KGL_EINVALID_PARAMETER;
	}
#ifdef ENABLE_BLACK_LIST
	case KD_REQ_CHECK_WHITE_LIST:
	{
		bool* flush = (bool*)ret;
		if (wlm.find(rq->sink->data.url->host, (char*)data, flush ? *flush : false)) {
			return KGL_OK;
		}
		return KGL_ENO_DATA;
	}
	case KD_REQ_ADD_WHITE_LIST:
	{
		wlm.add(rq->sink->data.url->host, NULL, (char*)data);
		return KGL_OK;
	}
#endif
	case KD_REQ_OBJ_IDENTITY:
	{
		if (rq->ctx.obj == NULL) {
			return KGL_ENO_DATA;
		}
		if (rq->ctx.obj->IsContentEncoding()) {
			return KGL_ENO_DATA;
		}
		rq->ctx.obj->uk.url->accept_encoding = (uint8_t)(~0);
		return KGL_OK;
	}
	default:
		break;
	}
	return KGL_ENOT_SUPPORT;
}
static KGL_RESULT support_function(
	KREQUEST r,
	KCONN                        cn,
	KF_REQ_TYPE                  req,
	PVOID                        data,
	PVOID* ret
)
{
	KHttpRequest* rq = (KHttpRequest*)r;
	switch (req) {
	case KF_REQ_UPSTREAM:
	{
		if (rq->ctx.obj) {
			//回应控制不允许注册upstream
			return KGL_EINVALID_PARAMETER;
		}
		kgl_upstream* us = (kgl_upstream*)data;
		//目前还不支持同步模式	
		KDsoRedirect* rd = new KDsoRedirect("", us);
		KRedirectSource* fo = rd->makeFetchObject(rq, *ret);
		fo->bindRedirect(rd, KGL_CONFIRM_FILE_NEVER);
		fo->filter = 1;
		if (KBIT_TEST(us->flags, KGL_UPSTREAM_BEFORE_CACHE)) {
			rq->insert_source(fo);
		} else {
			if (rq->fo_last && !rq->fo_last->filter) {
				delete fo;
				//EXSIT final source.
				return KGL_EEXSIT;
			}
			rq->append_source(fo);
		}
		return KGL_OK;
	}
	case KF_REQ_FILTER:
	{
		kgl_filter_body* body = new kgl_filter_body;
		body->filter = (kgl_filter*)data;
		body->ctx = (kgl_response_body_ctx*)*ret;
		rq->getOutputFilterContext()->add(body);
	}
	default:
		return base_support_function(rq, req, data, ret);
	}
}
static kgl_access_function request_access_function = {
	(kgl_get_variable_f)get_request_variable,
	support_function,
	response_unknow_header,
	set_request_header
};
static kgl_access_function response_access_function = {
	(kgl_get_variable_f)get_request_variable,
	//(kgl_get_variable_f)get_response_variable,
	support_function,
	add_obj_unknow_header,
	NULL
};
void init_access_dso_support(kgl_access_context* ctx, int notify)
{
	memset(ctx, 0, sizeof(kgl_access_context));
	if (KBIT_TEST(notify, KF_NOTIFY_REQUEST_ACL | KF_NOTIFY_REQUEST_MARK)) {
		//request
		ctx->f = &request_access_function;
		return;
	}
	ctx->f = &response_access_function;
}

static void request_queue_clean_call_back(void* data)
{
	char* key = (char*)data;
	std::map<char*, KRequestQueue*, lessp>::iterator it;
	queue_lock.Lock();
	it = queues.find(key);
	if (it != queues.end() && (*it).second->getRef() == 1) {
		//printf("free queue name:[%s]\n", key);
		free((*it).first);
		(*it).second->release();
		queues.erase(it);
	}
	queue_lock.Unlock();
	free(key);
}
KRequestQueue* get_request_queue(KHttpRequest* rq, const char* queue_name, int max_worker, int max_queue)
{
	KRequestQueue* queue = NULL;
	queue_lock.Lock();
	std::map<char*, KRequestQueue*, lessp>::iterator it = queues.find((char*)queue_name);
	if (it == queues.end()) {
		queue = new KRequestQueue;
		queues.insert(std::pair<char*, KRequestQueue*>(strdup(queue_name), queue));
	} else {
		queue = (*it).second;
	}
	queue->addRef();
	queue->set(max_worker, max_queue);
	queue_lock.Unlock();
	rq->registerRequestCleanHook(request_queue_clean_call_back, strdup(queue_name));
	return queue;
}
KRequestQueue* get_request_queue(KHttpRequest* rq, const char* queue_name)
{
	int max_worker = 1;
	int max_queue = 0;
	const char* hot = strchr(queue_name, '_');
	if (hot) {
		hot++;
		max_worker = atoi(hot);
		hot = strchr(hot, '_');
		if (hot) {
			hot++;
			max_queue = atoi(hot);
		}
	}
	return get_request_queue(rq, queue_name, max_worker, max_queue);
}
