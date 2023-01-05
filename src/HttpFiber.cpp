#include "kfiber.h"
#include "HttpFiber.h"
#include "KHttpRequest.h"
#include "http.h"
#include "cache.h"
#include "KStaticFetchObject.h"
#include "KSSLSniContext.h"
#include "KConfig.h"
#include "KVirtualHostManage.h"
#include "KHttpTransfer.h"
#include "KFetchBigObject.h"
#include "KBufferFetchObject.h"
#include "KBigObjectContext.h"
#include "KHttpServer.h"
#include "KDefaultFetchObject.h"

int stage_end_request(KHttpRequest* rq, KGL_RESULT result)
{
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		if (rq->bo_ctx->net_fiber != NULL) {
			return rq->bo_ctx->EndRequest(rq);
		}
		delete rq->bo_ctx;
		rq->bo_ctx = NULL;
	}
#endif
	return rq->EndRequest();
}

KGL_RESULT handle_x_send_file(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out)
{
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return send_error2(rq, STATUS_SERVER_ERROR, "X-Accel-Redirect cann't send body");
	}
	char* xurl = nullptr;
	bool x_proxy_redirect = false;
	KHttpHeader* header = rq->ctx->obj->data->headers;
	while (header) {
		if (xurl==nullptr && kgl_is_attr(header,_KS("X-Accel-Redirect"))) {
			xurl = kgl_strndup(header->buf+header->val_offset,header->val_len);
			x_proxy_redirect = false;
#if 0
		} else if (strcasecmp(header->attr, "X-Proxy-Redirect") == 0) {
			xurl = header->val;
			x_proxy_redirect = true;
#endif
		} else {
			rq->response_header(header, false);
		}
		header = header->next;
	}
	kassert(xurl != NULL);
	if (xurl == NULL) {
		return send_error2(rq, STATUS_SERVER_ERROR, "missing X-Accel-Redirect header");
	}
#if 0
	if (!rq->rewriteUrl(xurl, 0)) {
		return send_error2(rq, STATUS_SERVER_ERROR, "X-Accel-Redirect value is not right");
	}
#endif
	rq->ctx->internal = 1;
	rq->ctx->replace = 1;
	if (rq->file) {
		delete rq->file;
		rq->file = NULL;
	}
	char* filename = rq->map_url_path(xurl, nullptr);
	xfree(xurl);
	rq->ctx->clean_obj(rq);
	rq->ctx->obj = new KHttpObject(rq);
	//handleXSendfile no cache
	KBIT_SET(rq->ctx->obj->index.flags, FLAG_DEAD);
	rq->ctx->new_object = 1;
	if (filename == nullptr) {
		return send_error2(rq, STATUS_SERVER_ERROR, "X-Accel-Redirect cann't map url");
	}
	rq->file = new KFileName;
	bool result = rq->file->setName(filename);
	xfree(filename);
	if (!result) {
		return send_error2(rq, STATUS_NOT_FOUND, "No Such File.");
	}
	auto fo = new KDefaultFetchObject();
	rq->append_source(fo);
	return fo->Open(rq,in,out);
}
KGL_RESULT process_upstream_no_body(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out)
{
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
#if 0
		//big object		
		klog(KLOG_ERR, "unexpted bigobj net request http body.status_code=%d\n", rq->ctx->obj->data->status_code);
		return rq->bo_ctx->handleError(rq, STATUS_SERVER_ERROR, "Unexpeted http body");
#endif
		assert(false);

	}
#endif//}}
	if (!KBIT_TEST(rq->filter_flags, RQ_SWAP_OLD_OBJ) && KBIT_TEST(rq->ctx->obj->index.flags, ANSW_XSENDFILE)) {
		//TODO: handleXSendfile
		return handle_x_send_file(rq,in,out);
	}
	if (KBIT_TEST(rq->filter_flags, RQ_SWAP_OLD_OBJ)) {
		rq->ctx->pop_obj();
		KBIT_CLR(rq->filter_flags, RQ_SWAP_OLD_OBJ);
	}
	return send_memory_object(rq);
}
KGL_RESULT process_request(KHttpRequest* rq)
{
	kgl_input_stream* in = new_default_input_stream();
	kgl_output_stream* out = new_default_output_stream();
	KFetchObject* fo = rq->get_source();
	if (!fo) {
		assert(false);
		return send_error2(rq, STATUS_SERVER_ERROR, "source error!");
	}
	KGL_RESULT result = KGL_OK;
#ifdef ENABLE_TF_EXCHANGE
	if (rq->NeedTempFile(true)) {
		if (!new_tempfile_input_stream(rq, &in)) {
			KAutoReleaseStream st(in, out);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't read post");
		}
	}
	if (rq->NeedTempFile(false)) {
		if (!new_tempfile_output_stream(rq, &out)) {
			KAutoReleaseStream st(in, out);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't create temp file");
		}
	}
#endif
	KAutoReleaseStream st(in, out);
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue* queue = rq->queue;
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) || rq->ctx->internal || !rq->NeedQueue()) {
		//后台管理及内部调用，不用排队
		goto open;
	}	
	if (queue == NULL) {
		queue = &globalRequestQueue;
#ifdef ENABLE_VH_QUEUE
		auto svh = rq->get_virtual_host();
		if (svh && svh->vh->queue) {
			queue = svh->vh->queue;
		}
#endif
	}
	result = open_queued_fetchobj(rq, fo, in, out, queue);
	if (!rq->continue_next_source(result)) {
		goto done;
	}
	fo = rq->get_next_source(fo);
	if (!fo) {
		goto done;
	}
#endif
open:
	do {
		result = fo->Open(rq, in, out);
		if (!rq->continue_next_source(result)) {
			break;
		}
		fo = rq->get_next_source(fo);
	} while (fo);
done:
	switch (result) {
	case KGL_NO_BODY:
		return process_upstream_no_body(rq, in, out);
	case KGL_EDENIED:
		return send_error2(rq, STATUS_FORBIDEN, "access denied by response control");
	default:
		return result;
	}
}
KGL_RESULT open_queued_fetchobj(KHttpRequest* rq, KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, KRequestQueue* queue)
{

	if (queue && queue->getMaxWorker() > 0) {
		int64_t begin_lock_time = kgl_current_msec;
		if (!queue->Start(rq)) {
			return out->f->write_message(out, rq, KGL_MSG_ERROR, "Server is busy.", STATUS_SERVICE_UNAVAILABLE);
		}
		if (kgl_current_msec - begin_lock_time > conf.time_out * 1000) {
			rq->ReleaseQueue();
			return out->f->write_message(out, rq, KGL_MSG_ERROR,"Wait in queue time out.", STATUS_SERVICE_UNAVAILABLE);
		}
	}
	return fo->Open(rq, in, out);
}
query_vh_result query_virtual(KHttpRequest *rq, const char *hostname, int len, int header_length) {
	query_vh_result vh_result;
	KSubVirtualHost* svh = NULL;
#ifdef KSOCKET_SSL
	void* sni = rq->sink->get_sni();
	if (sni) {
		vh_result = kgl_use_ssl_sni(sni, &svh);
	} else
#endif
		vh_result = conf.gvm->queryVirtualHost((KVirtualHostContainer *)rq->sink->get_server_opaque(), &svh, hostname, len);
	rq->sink->data.bind_opaque(svh);
#ifdef ENABLE_VH_RS_LIMIT
	if (query_vh_success == vh_result && svh) {
		if (!svh->vh->addConnection(rq)) {
			vh_result = query_vh_connect_limit;
		}
	}
#endif
	return vh_result;
}
KGL_RESULT handle_denied_request(KHttpRequest *rq) 
{
	if (rq->has_final_source()) {
		/*
		if (rq->sink->data.status_code == 0) {
			rq->responseConnection();
			rq->responseStatus(STATUS_OK);
		}
		rq->startResponseBody(-1);
		*/
		return process_request(rq);
	}
	if (rq->sink->data.status_code > 0) {
		rq->start_response_body(0);
		return KGL_OK;
	}
	if (KBIT_TEST(rq->filter_flags, RQ_SEND_AUTH)) {
		return send_auth2(rq);
	}
	if (!KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return send_error2(rq, STATUS_FORBIDEN, "denied by request access control");
	}
	return KGL_OK;
}
bool check_virtual_host_access_request(KHttpRequest *rq, int header_length) {
	auto svh = rq->get_virtual_host();
	assert(svh);
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit *sl = svh->vh->refsSpeedLimit();
	if (sl) {
		rq->pushSpeedLimit(sl);
	}
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo *flow = svh->vh->refsFlowInfo();
	if (flow) {
		rq->pushFlowInfo(flow);
	}
#endif
	rq->sink->add_up_flow(header_length);
	if (svh->vh->closed) {
		handle_error(rq, STATUS_SERVICE_UNAVAILABLE, "virtual host is closed");
		return true;
	}
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	switch (svh->vh->checkRequest(rq)) {
	case JUMP_DROP:
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		//return stageEndRequest(rq);
	case JUMP_DENY:
		handle_denied_request(rq);
		return true;
	default:
		break;
	}
#endif
#endif
	return false;
}

KGL_RESULT handle_connect_method(KHttpRequest* rq)
{
	//{{ent
#ifdef HTTP_PROXY	
	if (rq->sink->data.url->host == NULL) {
		return KGL_EDATA_FORMAT;
	}
	if (rq->sink->data.raw_url->path == NULL) {
		rq->sink->data.raw_url->path = strdup("/");
	}
	if (rq->sink->data.url->path == NULL) {
		rq->sink->data.url->path = strdup("/");
	}
	switch (kaccess[REQUEST].check(rq, NULL)) {
	case JUMP_DROP:
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		return KGL_EDENIED;
	case JUMP_DENY:
		return handle_denied_request(rq);
	default:
		break;
	}
	if (!rq->HasFinalFetchObject()) {
		return send_error2(rq, STATUS_METH_NOT_ALLOWED, "CONNECT Not allowed.");
	}
	rq->sink->data.http_minor = 0;
	rq->ctx->obj = new KHttpObject(rq);
	rq->ctx->new_object = 1;
	return load_object(rq);
#endif//}}
	return send_error2(rq, STATUS_METH_NOT_ALLOWED, "The requested method CONNECT is not allowed");
}
void start_request_fiber(KSink *sink, int header_length)
{
	KHttpRequest *rq = new KHttpRequest(sink);
	KFetchObject* fo;
	rq->beginRequest();
#ifdef HTTP_PROXY
	if (rq->sink->data.meth == METH_CONNECT) {
		handle_connect_method(rq);
		goto clean;
	}
#endif
	if (unlikely(rq->ctx->read_huped)) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		send_error2(rq, STATUS_BAD_REQUEST, "Client close connection");
		goto clean;
	}
	if (unlikely(rq->isBad())) {
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		send_error2(rq, STATUS_BAD_REQUEST, "Bad request format.");
		goto clean;
	}
	if (unlikely(KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE))) {
		stageHttpManage(rq);
		goto clean;
	}
#ifdef ENABLE_STAT_STUB
	if (strcmp(rq->sink->data.url->path, "/kangle.status") == 0) {
		KAutoBuffer s(rq->sink->pool);
		if (rq->sink->data.meth != METH_HEAD) {
			s << "OK\n";
		}
		send_http2(rq, NULL, STATUS_OK, &s);
		goto clean;
	}
#endif
	if (rq->ctx->skip_access) {
		goto skip_access;
	}
	switch (kaccess[REQUEST].check(rq, NULL)) {
		case JUMP_DROP:
			goto clean;
		case JUMP_DENY:
			handle_denied_request(rq);
			goto clean;
		case JUMP_VHS: {
			query_vh_result vh_result = query_virtual(rq, rq->sink->data.url->host, 0, header_length);
			switch (vh_result) {
				case query_vh_connect_limit:
					send_error2(rq, STATUS_SERVER_ERROR, "max connect limit.");
					goto clean;
				case query_vh_host_not_found:
					send_error2(rq, STATUS_BAD_REQUEST, "host not found.");
					goto clean;
				case query_vh_success: {
					u_short flags = rq->sink->data.raw_url->flags;
					rq->sink->data.raw_url->flags = 0;
					if (check_virtual_host_access_request(rq, header_length)) {
						KBIT_SET(rq->sink->data.raw_url->flags, flags);
						goto clean;
					}
					if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_REWRITED)) {
						//rewrite host
						KSubVirtualHost *new_svh = NULL;
						conf.gvm->queryVirtualHost((KVirtualHostContainer*)rq->sink->get_server_opaque(), &new_svh, rq->sink->data.url->host, 0);
						if (new_svh) {
							auto svh = rq->get_virtual_host();
							if (new_svh->vh == svh->vh) {
								//只有虚拟主机相同，才允许重写host
								rq->sink->data.bind_opaque(new_svh);
							} else {
								new_svh->release();
							}
						}
					}
					KBIT_SET(rq->sink->data.raw_url->flags, flags);
					break;
				}
				default:
					send_error2(rq, STATUS_SERVER_ERROR, "query vh result unknow.");
					goto clean;
			}
		}
	}

skip_access:
	fo = rq->fo_head;
	while (fo && fo->before_cache) {
		KGL_RESULT result = fo->Open(rq, get_check_input_stream(), get_check_output_stream());
		fo = rq->get_next_source(fo);
		if (!rq->continue_next_source(result)) {
			goto clean;
		}
	}
	fiber_http_start(rq);
clean:
	if (kfiber_has_next()) {
		return;
	}
	stage_end_request(rq, KGL_OK);
	return;
}

KGL_RESULT handle_error(KHttpRequest *rq, int code, const char *msg) {

	if (code == 0) {
		rq->sink->shutdown();
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		return KGL_OK;
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		//TODO: big object handle error
		assert(false);
		return KGL_OK;
		//return rq->bo_ctx->handleError(rq, code, msg);
	}
#endif
#ifdef WORK_MODEL_TCP
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP | WORK_MODEL_TPROXY)) {
		return KGL_OK;
	}
#endif
#if 0
	if (KBIT_TEST(rq->filter_flags, RF_ALWAYS_ONLINE)) {
		//always on
		if (rq->ctx->old_obj) {
			//have cache
			rq->ctx->always_on_model = true;
			rq->ctx->pop_obj();
			if (JUMP_DENY == checkResponse(rq, rq->ctx->obj)) {
				return send_error2(rq, STATUS_FORBIDEN, "denied by response access");
			}
			return async_send_valide_object(rq, rq->ctx->obj);
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_MOD_SINCE | RQ_HAS_IF_NONE_MATCH)) {
			//treat as not-modified
			return send_not_modify_from_mem(rq, rq->ctx->obj);
		}
	}
#endif
	KSubVirtualHost* svh = rq->get_virtual_host();
	if (svh == NULL || code < 403 || code>499) {
		return send_error2(rq, code, msg);
	}
	KHttpObject *obj = rq->ctx->obj;
	if (obj) {
		obj->data->i.status_code = code;
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_IS_ERROR_PAGE)) {
		//如果本身是错误页面，又产生错误
		return send_error2(rq, code, msg);
	}
	//设置为错误页面
	KBIT_SET(rq->sink->data.flags, RQ_IS_ERROR_PAGE);
	//清除range请求
	KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
	assert(svh);
	std::string errorPage2;
	if (!svh->vh->getErrorPage(code, errorPage2)) {
		return send_error2(rq, code, msg);
	}
	const char *errorPage = errorPage2.c_str();
	if (strncasecmp(errorPage, "http://", 7) == 0 || strncasecmp(errorPage, "https://", 8) == 0) {
		std::stringstream s;
		s << errorPage << "?" << obj->data->i.status_code << "," << rq->getInfo();
		push_redirect_header(rq, s.str().c_str(), (int)s.str().size(), STATUS_FOUND);
		rq->start_response_body(0);
		return KGL_OK;
		//return stageWriteRequest(rq, NULL);
	}
	//string path;
	bool result = false;
	if (rq->file) {
		delete rq->file;
	}
	rq->file = new KFileName;
	/*
	skip_redirect 如果是本地文件file:// 则不用查找扩展映射，当普通文件即可.
	*/
	//bool skip_redirect = true;
	if (strncasecmp(errorPage, "file://", 7) == 0) {
		errorPage += 7;
		if (isAbsolutePath(errorPage)) {
			result = rq->file->setName(errorPage);
		} else {
			result = rq->file->setName(conf.path.c_str(), errorPage, rq->getFollowLink());
		}
	} else {
		//skip_redirect = false;
	//	path = rq->svh->doc_root;
		KStringBuf errorUrl;
		if (errorPage[0] != '/') {
			char *url = (char *)xstrdup(rq->sink->data.url->path);
			char *p = strrchr(url, '/');
			if (p) {
				*(p + 1) = '\0';
			}
			errorUrl << url << errorPage;
			xfree(url);
		} else {
			errorUrl << errorPage;
		}
		result = rq->file->setName(svh->doc_root, errorPage, rq->getFollowLink());
		if (result) {
			result = rq->rewrite_url(errorUrl.getString(), code);
		}
	}
	if (result && !rq->file->isDirectory()) {
		bool redirect_result;
		KFetchObject *fo = svh->vh->findFileExtRedirect(rq, rq->file, true, redirect_result);
		if (fo == NULL) {
			fo = new KStaticFetchObject;
		}
		rq->append_source(fo);
		return process_request(rq);
	}
	return send_error2(rq, code, msg);
	//*/
}
/*
准备读文件，分捡请求
*/
KGL_RESULT prepare_request_fetchobj(KHttpRequest *rq)
{
	assert(rq->ctx->obj);
	RequestError error;
	error.code = STATUS_SERVER_ERROR;
	error.msg = "internal error";
	if (!rq->has_final_source()) {
		if (rq->sink->data.opaque == NULL) {
#ifdef ENABLE_VH_RS_LIMIT
			return send_error2(rq, STATUS_SERVER_ERROR, "access action error");
#else
			if (query_vh_success != conf.gvm->queryVirtualHost(rq->c->ls, &rq->svh, rq->sink->data.url->host)) {
				return send_error2(rq, NULL, STATUS_BAD_REQUEST, "host not found.");
			}
#endif
		}
		KAccess *htresponse = NULL;
		bool handled = false;
		KFetchObject *fo = bindVirtualHost(rq, &error, &htresponse, handled);
		if (handled) {
			//已经处理了
			assert(fo == NULL);
			return KGL_OK;
		}
		if (fo) {
			rq->append_source(fo);
		}
		//postmap
		if (htresponse) {
			if (!rq->ctx->internal && !rq->ctx->replace	&& htresponse->checkPostMap(rq, rq->ctx->obj) == JUMP_DENY) {
				delete htresponse;
				if (KBIT_TEST(rq->filter_flags, RQ_SEND_AUTH)) {
					return send_auth2(rq);
				}
				return handle_error(rq, STATUS_FORBIDEN, "Deny by htaccess file");
			}
			delete htresponse;
		}
	}
	if (!rq->ctx->internal && !rq->ctx->replace && !rq->ctx->skip_access) {
		KSubVirtualHost* svh = rq->get_virtual_host();
		if (svh) {
			if (svh->vh->checkPostMap(rq) == JUMP_DENY) {
				if (KBIT_TEST(rq->filter_flags, RQ_SEND_AUTH)) {
					return send_auth2(rq);
				}
				return handle_error(rq, STATUS_FORBIDEN, "Deny by vh postmap access");
			}
		}
		if (kaccess[RESPONSE].checkPostMap(rq, rq->ctx->obj) == JUMP_DENY) {
			if (KBIT_TEST(rq->filter_flags, RQ_SEND_AUTH)) {
				return send_auth2(rq);
			}
			return send_error2(rq, STATUS_FORBIDEN, "Deny by global postmap access");
		}
	}
	if (!rq->has_final_source()) {
		return handle_error(rq, error.code, error.msg);
	}
	return process_request(rq);
}
KGL_RESULT load_object(KHttpRequest *rq)
{
	KContext *context = rq->ctx;
	context->mt = modified_if_modified;
	if (rq->sink->data.if_modified_since > 0) {
		if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_DATE)) {
			context->mt = modified_if_range_date;
		}
	} else if (rq->sink->data.if_none_match) {
		context->mt = modified_if_none_match;
		if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_ETAG)) {
			context->mt = modified_if_range_etag;
		}
	} else if (context->old_obj && context->old_obj->data && !KBIT_TEST(context->old_obj->index.flags, OBJ_NOT_OK)) {
		if (context->old_obj->data->i.last_modified > 0) {
			rq->sink->data.if_modified_since = context->old_obj->data->i.last_modified;
		} else if (KBIT_TEST(context->old_obj->index.flags, OBJ_HAS_ETAG)) {
			KHttpHeader* h = context->old_obj->findHeader(_KS("Etag"));
			if (h) {
				context->mt = modified_if_none_match;
				rq->sink->set_if_none_match(h->buf, h->val_len);
			}
		} else {
			rq->sink->data.if_modified_since = context->obj->index.last_verified;
		}
	}
	return prepare_request_fetchobj(rq);
}

/**
* 发送在内存中的object.
*/
KGL_RESULT send_memory_object(KHttpRequest *rq)
{
	//printf("send memory object obj=[%p]\n", rq->ctx->obj);
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->ctx->obj->data->i.type == BIG_OBJECT_PROGRESS) {
		if (rq->bo_ctx == NULL) {
			rq->bo_ctx = new KBigObjectContext(rq->ctx->obj);
			return rq->bo_ctx->Open(rq, false);
		}
		assert(false);
		//return rq->bo_ctx->send(rq);
	}
#endif
#ifdef ENABLE_BIG_OBJECT
	if (rq->ctx->obj->data->i.type == BIG_OBJECT) {
		KFetchObject* fo = new KFetchBigObject();
		rq->append_source(fo);
		return fo->Open(rq,NULL,NULL);
	}
#endif
	INT64 send_len;
	INT64 start;
	kbuf *send_buffer = build_memory_obj_header(rq, rq->ctx->obj, start, send_len);
	if (KBIT_TEST(rq->ctx->obj->index.flags, FLAG_NO_BODY) || rq->sink->data.meth == METH_HEAD) {
		send_len = 0;
		return KGL_NO_BODY;
	}
	if (send_buffer && send_len>0) {
		kr_buffer buffer;
		memset(&buffer, 0, sizeof(buffer));
		kr_init(&buffer, send_buffer, (int)start, (int)send_len, rq->sink->pool);
		WSABUF buf[16];
		for (;;) {
			int bc = kr_get_read_buffers(&buffer, buf, sizeof(buf) / sizeof(WSABUF));
			int got = rq->Write(buf, bc);
			if (got <= 0) {
				return KGL_ESOCKET_BROKEN;
			}
			if (!kr_read_success(&buffer, got)) {
				return KGL_OK;
			}
		}
	}
	return KGL_OK;
}
/*
obj is not expire
*/
KGL_RESULT send_cache_object(KHttpRequest *rq, KHttpObject *obj)
{
	//rq->sink->data.status_code = STATUS_OK;
	bool not_modifed = false;
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_MOD_SINCE | RQ_IF_RANGE_DATE)) {
		time_t useTime = obj->data->i.last_modified;
		if (useTime <= 0) {
			useTime = obj->index.last_verified;
		}
		if (rq->sink->data.if_modified_since >= useTime) {
			//not change
			if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_MOD_SINCE)) {
				not_modifed = true;
				//rq->sink->data.status_code = STATUS_NOT_MODIFIED;
			}
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_DATE)) {
			KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
			rq->sink->data.range_from = 0;
			rq->sink->data.range_to = -1;
		}
	} else if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_NONE_MATCH)) {
		kgl_str_t *if_none_match = rq->sink->data.if_none_match;
		if (if_none_match && obj->matchEtag(if_none_match->data, (int)if_none_match->len)) {
			not_modifed = true;
			//rq->sink->data.status_code = STATUS_NOT_MODIFIED;
		}
		rq->sink->clean_if_none_match();
	} else if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_ETAG)) {
		kgl_str_t *if_none_match = rq->sink->data.if_none_match;
		if (if_none_match == NULL || !obj->matchEtag(if_none_match->data, (int)if_none_match->len)) {
			KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
			rq->sink->data.range_from = 0;
			rq->sink->data.range_to = -1;
		}
		rq->sink->clean_if_none_match();
	}
	if (not_modifed) {
		return send_http2(rq, obj, STATUS_NOT_MODIFIED);
	}
	return send_memory_object(rq);
}

swap_in_result swap_in_object(KHttpRequest *rq, KHttpObject *obj)
{
	if (!KBIT_TEST(obj->index.flags, FLAG_IN_MEM)) {
		KMutex *lock = obj->getLock();
		//rq->c->removeRequest(rq,true);
		lock->Lock();
		if (obj->data == NULL) {
			if (cache.IsCleanBlocked()) {
				lock->Unlock();
				return swap_in_failed_clean_blocked;
				//return process_cache_ready_request(rq, obj, swap_in_failed_clean_blocked);
			}
#ifdef ENABLE_DISK_CACHE
			KHttpObjectSwaping *obj_swap = new KHttpObjectSwaping;
			obj->data = new KHttpObjectBody();
			obj->data->i.type = SWAPING_OBJECT;
			obj->data->os = obj_swap;
			//obj->data->os->addTask(rq, processCacheReadyRequest);
			lock->Unlock();
			return obj_swap->swapin(rq, obj);
#else
			lock->Unlock();
			KBIT_SET(obj->index.flags, FLAG_DEAD);
			klog(KLOG_ERR, "BUG!! obj is not in memory.");
			assert(false);
			return swap_in_failed_other;
#endif
#ifdef ENABLE_DISK_CACHE
		} else if (obj->data->i.type == SWAPING_OBJECT) {
			//已经有其它线程在swap
			KHttpObjectSwaping *os = obj->data->os;
			assert(os);
			return os->wait(lock);
#endif
		}
		lock->Unlock();
	}
	return swap_in_success;
}

KGL_RESULT process_cache_request(KHttpRequest *rq) {
	KHttpObject *obj = rq->ctx->obj;
	swap_in_result result = swap_in_object(rq, obj);
	switch (result) {
	case swap_in_busy:
		klog(KLOG_ERR, "obj swap in busy drop request.\n");
		return send_error2(rq, STATUS_SERVER_ERROR, "swap in busy");
	case swap_in_success:
		KBIT_SET(rq->sink->data.flags, RQ_CACHE_HIT);
		return process_check_cache_expire(rq, obj);
	default: 
		{
			//swap in failed.
	#ifdef ENABLE_DISK_CACHE
		//不能swap in就从源上去取
			char *filename = obj->getFileName();
			klog(KLOG_ERR, "obj swap in failed cache file [%s] error=[%d].\n", filename, result);
			free(filename);
	#endif
			rq->ctx->clean_obj(rq);
			rq->ctx->new_object = true;
			rq->sink->data.if_modified_since = 0;
			rq->ctx->obj = new KHttpObject(rq);
			if (swap_in_failed_clean_blocked == result) {
				KBIT_SET(rq->ctx->obj->index.flags, FLAG_DEAD);
			}
			if (rq->file) {
				delete rq->file;
				rq->file = NULL;
			}
			assert(!rq->is_source_empty() || KBIT_TEST(rq->sink->get_server_model(), WORK_MODEL_MANAGE) || rq->sink->data.opaque);
			return load_object(rq);
		}
	}
}
KGL_RESULT fiber_http_start(KHttpRequest *rq)
{
	KContext *context = rq->ctx;
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		printf("TODO: bigobject\n");
		assert(false);
		//return stage_prepare(rq);
		return prepare_request_fetchobj(rq);
	}
#endif//}}
#ifdef ENABLE_STAT_STUB
	if (conf.max_io > 0 && katom_get((void *)&kgl_aio_count) > (uint32_t)conf.max_io) {
		//async io limit
		KBIT_SET(rq->filter_flags, RF_NO_DISK_CACHE);
	}
#endif
	//only if cached
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_ONLY_IF_CACHED)) {
		context->obj = findHttpObject(rq, false, context);
		if (!context->obj) {
			return send_error2(rq, 404, "Not in cache");
		}
		return process_cache_request(rq);
	}
	//end purge or only if cached
	if (in_stop_cache(rq)) {
		context->obj = new KHttpObject(rq);
		context->new_object = 1;
		if (!context->obj) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
		KBIT_SET(context->obj->index.flags, FLAG_DEAD);
	} else {
		context->obj = findHttpObject(rq, true, context);
		if (!context->obj) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
	}
	if (context->new_object) { //It is a new object
		if (rq->sink->data.meth != METH_GET) {
			KBIT_SET(context->obj->index.flags, FLAG_DEAD);
		}
		return load_object(rq);
	}
	return process_cache_request(rq);
}
/**
异步load body准备
*/
KGL_RESULT prepare_write_body(KHttpRequest *rq)
{
	prepare_write_stream(rq);
	if (KBIT_TEST(rq->sink->data.flags, RQ_CONNECTION_UPGRADE)
#ifdef WORK_MODEL_TCP		
		&& !KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP)
#endif
		) {
		//非tcp的转发双通道，应该立即发送http头到客户端。
		//tcp不需要http回应头
		return rq->tr->sendHead(rq, false);		
	}
	return KGL_OK;
}

KGL_RESULT on_upstream_finished_header(KHttpRequest *rq)
{
	KContext *context = rq->ctx;
	//KHttpRequest *rq = context->rq;
	KHttpObject *obj = rq->ctx->obj;
	if (obj->data->i.status_code == 0) {
		//如果status没有设置，设置为200
		obj->data->i.status_code = STATUS_OK;
	}
	uint16_t status_code = obj->data->i.status_code;
	if (status_code != STATUS_OK && status_code != STATUS_CONTENT_PARTIAL) {
		KBIT_SET(obj->index.flags, ANSW_NO_CACHE | OBJ_NOT_OK);
	}
	if (checkResponse(rq, obj) == JUMP_DENY) {
		return KGL_EDENIED;
	}
	obj->checkNobody();
	switch (status_code) {
	case STATUS_NOT_MODIFIED:		
		KBIT_SET(obj->index.flags, FLAG_DEAD);
		if (context->old_obj) {
			if (!KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_MOD_SINCE | RQ_HAS_IF_NONE_MATCH)) {
				//直接发送old_obj给客户
				KBIT_SET(rq->filter_flags, RQ_SWAP_OLD_OBJ);
			}
			KBIT_SET(rq->sink->data.flags, RQ_OBJ_VERIFIED);
			context->old_obj->index.last_verified = kgl_current_sec;
			context->old_obj->UpdateCache(obj);
		} else {
			rq->ctx->no_body = true;
		}
		kassert(obj->data->bodys == NULL);
		return KGL_NO_BODY;
		//return return KGL_NO_BODY;(rq);
	default:
		KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
		if (rq->sink->data.meth == METH_HEAD || KBIT_TEST(context->obj->index.flags, FLAG_NO_BODY)) {
			//没有http body的情况
			KBIT_SET(obj->index.flags, FLAG_DEAD);
			rq->ctx->no_body = true;
			return KGL_NO_BODY;
		}
#ifdef ENABLE_BIG_OBJECT_206
		if (rq->bo_ctx) {
			bool big_obj_dead;
			KGL_RESULT result = rq->bo_ctx->upstream_recv_headed(rq, obj,big_obj_dead);
			if (!big_obj_dead || result!=KGL_OK) {
				return result;
			}
		}
		assert(obj->data->i.type == MEMORY_OBJECT);
		//普通请求
		if (rq->sink->data.meth == METH_GET
			&& conf.cache_part
			&& obj_can_disk_cache(rq, obj)
			&& obj->getTotalContentSize(rq) < conf.max_bigobj_size
			&& objCanCache(rq, obj)) {
			if (obj->getTotalContentSize(rq) >= conf.max_cache_size) {
				if (KBIT_TEST(rq->ctx->obj->index.flags, ANSW_HAS_CONTENT_LENGTH)) {
					return turn_on_bigobject(rq, obj);
				}
			}
		}
#endif
		rq->ctx->dead_old_obj();
		if (status_code == STATUS_CONTENT_PARTIAL && !obj->IsContentRangeComplete(rq)) {
			//强行设置206不缓存
			KBIT_SET(obj->index.flags, ANSW_NO_CACHE | OBJ_NOT_OK);
		} else {
			KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
		}
		kassert(!kfiber_is_main());
		return prepare_write_body(rq);
	}
}
