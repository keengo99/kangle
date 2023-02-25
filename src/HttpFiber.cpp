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
#include "KDefer.h"
#include "KFilterContext.h"

int stage_end_request(KHttpRequest* rq, KGL_RESULT result) {
	return rq->EndRequest();
}

KGL_RESULT handle_x_send_file(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return send_error2(rq, STATUS_SERVER_ERROR, "X-Accel-Redirect cann't send body");
	}
	char* xurl = nullptr;
	bool x_proxy_redirect = false;
	KHttpHeader* header = rq->ctx.obj->data->headers;
	while (header) {
		if (xurl == nullptr && kgl_is_attr(header, _KS("X-Accel-Redirect"))) {
			xurl = kgl_strndup(header->buf + header->val_offset, header->val_len);
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
	rq->ctx.internal = 1;
	if (rq->file) {
		delete rq->file;
		rq->file = NULL;
	}
	char* filename = rq->map_url_path(xurl, nullptr);
	xfree(xurl);
	rq->clean_obj();
	rq->ctx.obj = new KHttpObject(rq);
	//handleXSendfile no cache
	KBIT_SET(rq->ctx.obj->index.flags, FLAG_DEAD);
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
	return fo->Open(rq, in, out);
}

KGL_RESULT process_upstream_no_body(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	if (KBIT_TEST(rq->ctx.obj->index.flags, ANSW_XSENDFILE)) {
		return handle_x_send_file(rq, in, out);
	}
	if (kgl_load_response_body(rq, nullptr)) {
		return KGL_OK;
	}
	return KGL_ESOCKET_BROKEN;
}
KGL_RESULT process_request(KHttpRequest* rq) {
	kgl_input_stream in;
	kgl_output_stream out;
	new_default_stream(rq, &in, &out);
	defer(in.f->body.close(in.body_ctx); out.f->close(out.ctx));
	KGL_RESULT result = process_request_stream(rq, &in, &out);
	if (result == KGL_NO_BODY) {
		return process_upstream_no_body(rq, &in, &out);
	}
	return result;
}
KGL_RESULT process_request_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	KFetchObject* fo = rq->get_source();
	KGL_RESULT result = KGL_OK;
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue* queue = rq->queue;
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) || rq->ctx.internal || !rq->NeedQueue()) {
		//后台管理及内部调用，不用排队
		goto skip_queue;
	}
#ifdef ENABLE_VH_QUEUE
	if (queue == NULL) {
		auto svh = rq->get_virtual_host();
		if (svh && svh->vh->queue) {
			queue = svh->vh->queue;
		}
	}
#endif
	if (queue && fo) {
		result = open_queued_fetchobj(rq, fo, in, out, queue);
		if (!rq->continue_next_source(result)) {
			goto done;
		}
		fo = fo->next;

	}
#endif
skip_queue:
	while (fo) {
		result = fo->Open(rq, in, out);
		if (!rq->continue_next_source(result)) {
			break;
		}
		fo = fo->next;
	}
done:
	return result;
}
KGL_RESULT open_queued_fetchobj(KHttpRequest* rq, KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, KRequestQueue* queue) {
	if (!queue) {
		return fo->Open(rq, in, out);
	}
	int64_t begin_lock_time = kgl_current_msec;
	if (!queue->start()) {
		return out->f->error(out->ctx, STATUS_SERVICE_UNAVAILABLE, _KS("Server is busy."));
	}
	defer(queue->stop());
	if (kgl_current_msec - begin_lock_time > conf.time_out * 1000) {
		return out->f->error(out->ctx, STATUS_SERVICE_UNAVAILABLE, _KS("Wait in queue time out."));
	}
	return fo->Open(rq, in, out);
}
query_vh_result query_virtual(KHttpRequest* rq, const char* hostname, int len, int header_length) {
	query_vh_result vh_result;
	KSubVirtualHost* svh = NULL;
#ifdef KSOCKET_SSL
	void* sni = rq->sink->get_sni();
	if (sni) {
		vh_result = kgl_use_ssl_sni(sni, &svh);
	} else
#endif
		vh_result = conf.gvm->queryVirtualHost((KVirtualHostContainer*)rq->sink->get_server_opaque(), &svh, hostname, len);
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
KGL_RESULT handle_denied_request(KHttpRequest* rq) {
	KFetchObject* fo = rq->fo_head;
	while (fo && fo->filter) {
		fo = fo->next;
	}
	if (fo) {
		kgl_output_stream out;
		get_check_output_stream(rq, &out);
		return fo->Open(rq, nullptr, &out);
	}
	if (rq->sink->data.status_code > 0) {
		rq->start_response_body(0);
		return KGL_OK;
	}
	if (KBIT_TEST(rq->ctx.filter_flags, RQ_SEND_AUTH)) {
		return send_auth2(rq);
	}
	if (!KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return send_error2(rq, STATUS_FORBIDEN, "denied by request access control");
	}
	return KGL_OK;
}
bool check_virtual_host_access_request(KHttpRequest* rq,KFetchObject **fo, int header_length) {
	auto svh = rq->get_virtual_host();
	assert(svh);
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit* sl = svh->vh->refsSpeedLimit();
	if (sl) {
		rq->pushSpeedLimit(sl);
	}
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo* flow = svh->vh->refsFlowInfo();
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
	switch (svh->vh->checkRequest(rq, fo)) {
	case JUMP_DROP:
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		if (fo && *fo) {
			delete *fo;
			*fo = nullptr;
		}
	case JUMP_DENY:
		if (fo && *fo) {
			rq->append_source(*fo);
			*fo = nullptr;
		}
		handle_denied_request(rq);
		return true;
	default:
		break;
	}
#endif
#endif
	return false;
}

KGL_RESULT handle_connect_method(KHttpRequest* rq) {
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
	if (!rq->has_final_source()) {
		return send_error2(rq, STATUS_METH_NOT_ALLOWED, "CONNECT Not allowed.");
	}
	rq->sink->data.http_minor = 0;
	rq->ctx.obj = new KHttpObject(rq);
	rq->ctx.new_object = 1;
	return load_object(rq);
#endif
	return send_error2(rq, STATUS_METH_NOT_ALLOWED, "The requested method CONNECT is not allowed");
}
void start_request_fiber(KSink* sink, int header_length) {
	KHttpRequest* rq = new KHttpRequest(sink);
	kgl_input_stream check_in;
	kgl_output_stream check_out;
	KFetchObject* fo = nullptr;
	kgl_jump_type jump_type;
	rq->beginRequest();
#ifdef HTTP_PROXY
	if (rq->sink->data.meth == METH_CONNECT) {
		handle_connect_method(rq);
		goto clean;
	}
#endif
	if (unlikely(rq->ctx.read_huped)) {
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
	if (rq->ctx.skip_access) {
		goto skip_access;
	}
	jump_type = kaccess[REQUEST].check(rq, NULL, &fo);
	if (fo) {
		rq->append_source(fo);
	}
	switch (jump_type) {
	case JUMP_DROP:
		goto clean;
	case JUMP_DENY:
		handle_denied_request(rq);
		goto clean;
	case JUMP_VHS: {
		assert(fo == nullptr);
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
			fo = nullptr;
			if (check_virtual_host_access_request(rq, &fo, header_length)) {
				KBIT_SET(rq->sink->data.raw_url->flags, flags);
				goto clean;
			}
			if (fo) {
				rq->append_source(fo);
			}
			if (KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_REWRITED)) {
				//rewrite host
				KSubVirtualHost* new_svh = NULL;
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
	while (fo && fo->before_cache()) {
		get_check_stream(rq, &check_in, &check_out);
		KGL_RESULT result = fo->Open(rq, &check_in, &check_out);
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

KGL_RESULT handle_error(KHttpRequest* rq, int code, const char* msg) {

	if (code == 0) {
		rq->sink->shutdown();
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
		return KGL_OK;
	}
#ifdef WORK_MODEL_TCP
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP | WORK_MODEL_TPROXY)) {
		return KGL_OK;
	}
#endif
#if 0
	if (KBIT_TEST(rq->ctx.filter_flags, RF_ALWAYS_ONLINE)) {
		//always on
		if (rq->ctx.old_obj) {
			//have cache
			rq->ctx.always_on_model = true;
			rq->ctx.pop_obj();
			if (JUMP_DENY == checkResponse(rq, rq->ctx.obj)) {
				return send_error2(rq, STATUS_FORBIDEN, "denied by response access");
			}
			return async_send_valide_object(rq, rq->ctx.obj);
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_IF_MOD_SINCE | RQ_HAS_IF_NONE_MATCH)) {
			//treat as not-modified
			return send_not_modify_from_mem(rq, rq->ctx.obj);
		}
	}
#endif
	KSubVirtualHost* svh = rq->get_virtual_host();
	if (svh == NULL || code < 403 || code>499) {
		return send_error2(rq, code, msg);
	}
	KHttpObject* obj = rq->ctx.obj;
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
	rq->sink->data.range = nullptr;
	//KBIT_CLR(rq->sink->data.flags, RQ_HAVE_RANGE);
	assert(svh);
	std::string errorPage2;
	if (!svh->vh->getErrorPage(code, errorPage2)) {
		return send_error2(rq, code, msg);
	}
	const char* errorPage = errorPage2.c_str();
	if (strncasecmp(errorPage, "http://", 7) == 0 || strncasecmp(errorPage, "https://", 8) == 0) {
		std::stringstream s;
		s << errorPage << "?" << obj->data->i.status_code << "," << rq->getInfo();
		push_redirect_header(rq, s.str().c_str(), (int)s.str().size(), STATUS_FOUND);
		rq->response_content_length(0);
		rq->start_response_body(0);
		return KGL_OK;
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
			char* url = (char*)xstrdup(rq->sink->data.url->path);
			char* p = strrchr(url, '/');
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
			result = rq->rewrite_url(errorUrl.c_str(), code);
		}
	}
	if (result && !rq->file->isDirectory()) {
		bool redirect_result;
		KFetchObject* fo = svh->vh->findFileExtRedirect(rq, rq->file, true, redirect_result);
		if (fo == NULL) {
			fo = new KStaticFetchObject;
		}
		rq->append_source(fo);
		return process_request(rq);
	}
	return send_error2(rq, code, msg);
}
bool check_request_final_source(KHttpRequest* rq, RequestError* error) {
	if (!rq->has_final_source()) {
		if (rq->sink->data.opaque == NULL) {
#ifdef ENABLE_VH_RS_LIMIT
			error->code = STATUS_SERVER_ERROR;
			error->msg = "access action error";
			return false;
#else
			if (query_vh_success != conf.gvm->queryVirtualHost(rq->c->ls, &rq->svh, rq->sink->data.url->host)) {
				error->code = STATUS_BAD_REQUEST;
				error->msg = "host not found.";
				return false;
			}
#endif
		}
		KAccess* htresponse = NULL;
		KFetchObject* fo = bindVirtualHost(rq, error, &htresponse);
		if (!fo) {
			if (htresponse) {
				delete htresponse;
			}
			return false;
		}
		rq->append_source(fo);
		//postmap
		if (htresponse) {
			if (!rq->ctx.internal && htresponse->checkPostMap(rq, rq->ctx.obj, NULL) == JUMP_DENY) {
				delete htresponse;
				if (KBIT_TEST(rq->ctx.filter_flags, RQ_SEND_AUTH)) {
					error->code = AUTH_STATUS_CODE;
					error->msg = "";
					return false;
				}
				error->msg = "Deny by htaccess file";
				error->code = STATUS_FORBIDEN;
				return false;
			}
			delete htresponse;
		}
	}
	if (!rq->ctx.internal && !rq->ctx.skip_access) {
		KSubVirtualHost* svh = rq->get_virtual_host();
		if (svh) {
			KFetchObject* fo = nullptr;
			int jump_type = svh->vh->checkPostMap(rq, &fo);
			assert(!fo);
			if (fo) {
				delete fo;
			}
			if (jump_type== JUMP_DENY) {
				if (KBIT_TEST(rq->ctx.filter_flags, RQ_SEND_AUTH)) {
					error->code = AUTH_STATUS_CODE;
					error->msg = "";
					return false;
				}
				error->code = STATUS_FORBIDEN;
				error->msg = "Deny by vh postmap access";
				return false;
			}
		}
		if (kaccess[RESPONSE].checkPostMap(rq, rq->ctx.obj, NULL) == JUMP_DENY) {
			if (KBIT_TEST(rq->ctx.filter_flags, RQ_SEND_AUTH)) {
				error->code = AUTH_STATUS_CODE;
				error->msg = "";
				return false;
			}
			error->code = STATUS_FORBIDEN;
			error->msg = "Deny by global postmap access";
			return false;
		}
	}
	if (!rq->has_final_source()) {
		error->code = STATUS_SERVER_ERROR;
		error->msg = "no final source";
		return false;
	}
	return true;
}
bool process_check_final_source(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out, KGL_RESULT* result) {
	RequestError error;
	error.code = STATUS_SERVER_ERROR;
	error.msg = "internal error";
	if (check_request_final_source(rq, &error)) {
		return true;
	}	
	if (error.code == AUTH_STATUS_CODE) {
		*result = send_auth2(rq);
		return false;
	}
	*result = handle_error(rq, error.code, error.msg);
	return false;
}
/*
准备读文件，分捡请求
*/
KGL_RESULT prepare_request_fetchobj(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	assert(rq->ctx.obj);
	KGL_RESULT result;
	if (!process_check_final_source(rq, in, out, &result)) {
		return result;
	}
	return process_request_stream(rq, in, out);
}
void merge_precondition(KHttpRequest* rq, KHttpObject* obj) {
	kgl_precondition_flag flag;
	kgl_precondition* condition = rq->sink->get_precondition(&flag);
	assert(obj->data);
	kgl_sub_request* sub_request = rq->ctx.sub_request;
	assert(sub_request);
	if (obj->data->etag) {
		sub_request->precondition = rq->sink->alloc<kgl_precondition>();
		if (obj->data->i.condition_is_time) {
			if (condition && (KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_modified_since) && condition->time > obj->data->last_modified) {
				sub_request->precondition->time = condition->time;
			} else {
				sub_request->precondition->time = obj->data->last_modified;
			}
			rq->ctx.precondition_flag = kgl_precondition_if_modified_since;
		} else {
			auto etag = obj->data->etag;
			if (condition && KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_none_match) {
				size_t new_len = etag->len + condition->entity->len + 1;
				sub_request->precondition->entity = (kgl_len_str_t*)kgl_pnalloc(rq->sink->pool, new_len + 1 + sizeof(kgl_len_str_t));
				sub_request->precondition->entity->len = new_len;
				char* hot = sub_request->precondition->entity->data;
				memcpy(hot, etag->data, etag->len);
				hot += etag->len;
				*hot = ',';
				hot++;
				memcpy(hot, condition->entity->data, condition->entity->len);
				hot[condition->entity->len] = '\0';
			} else {
				sub_request->precondition->entity = etag;
			}
			rq->ctx.precondition_flag = kgl_precondition_if_none_match;
		}
	} else {
		sub_request->precondition = condition;
		rq->ctx.precondition_flag = KBIT_TEST(flag, kgl_precondition_mask);
	}
}
KGL_RESULT load_object_from_source(KHttpRequest* rq) {
	KHttpObject* old_obj = rq->ctx.old_obj;
	if (old_obj && !KBIT_TEST(old_obj->index.flags, OBJ_NOT_OK)) {
		assert(old_obj->data);
		kgl_sub_request* sub_request = rq->alloc_sub_request();
		merge_precondition(rq, old_obj);
		if (!kgl_request_match_if_range(rq, old_obj)) {
			sub_request->range = rq->sink->data.range;
			if (KBIT_TEST(rq->sink->data.flags, RQ_IF_RANGE_DATE)) {
				rq->ctx.precondition_flag |= kgl_precondition_if_range_date;
			}
		}
	}
	kgl_input_stream in;
	kgl_output_stream out;
	new_default_stream(rq, &in, &out);
	defer(in.f->body.close(in.body_ctx); out.f->close(out.ctx));
	KGL_RESULT result = prepare_request_fetchobj(rq, &in, &out);
	if (result != KGL_NO_BODY) {
		return result;
	}
	if (!rq->ctx.old_obj || rq->ctx.obj->data->i.status_code != STATUS_NOT_MODIFIED) {
		goto done;
	}
	if (rq->ctx.obj->data->etag && !rq->ctx.obj->is_same_precondition(rq->ctx.old_obj)) {
		goto done;
	}
	if (!kgl_request_precondition(rq, rq->ctx.old_obj)) {
		return send_http2(rq, rq->ctx.old_obj, STATUS_NOT_MODIFIED);
	}
	KBIT_SET(rq->sink->data.flags, RQ_OBJ_VERIFIED);
	rq->ctx.old_obj->index.last_verified = kgl_current_sec;
	rq->ctx.old_obj->UpdateCache(rq->ctx.obj);
	rq->pop_obj();
	return send_memory_object(rq);
done:
	KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
	rq->dead_old_obj();
	return process_upstream_no_body(rq, &in, &out);
}
KGL_RESULT process_check_cache_expire(KHttpRequest* rq, KHttpObject* obj) {
#ifdef ENABLE_BIG_OBJECT_206
	if (obj->data->i.type == BIG_OBJECT_PROGRESS) {
		/**
		* 未完物件，特殊处理。或产生if-range请求头会验证是否修改。
		*
		*/
		KBigObjectContext* bo_ctx = new KBigObjectContext(rq, obj);
		return bo_ctx->close(bo_ctx->open_cache());
	}
#endif
	if (!is_cache_object_expired(rq, obj)) {
		return send_cache_object(rq, obj);
	}
	return load_object_from_source(rq);
}
KGL_RESULT response_cache_object(KHttpRequest* rq, KHttpObject* obj) {
	int64_t content_length = obj->index.content_length;
	rq->ctx.left_read = content_length;
	kgl_load_cache_response_body(rq, &content_length);
	int64_t start = 0;
	KGL_RESULT result = KGL_OK;
	bool build_status = true;
	if (rq->sink->data.range &&
		content_length > 0 &&
		obj->data->i.status_code == STATUS_OK) {
		if (!rq->sink->adjust_range(&rq->ctx.left_read)) {
			rq->response_status(STATUS_RANGE_NOT_SATISFIABLE);
			rq->response_content_range(nullptr, content_length);
			rq->response_header(kgl_header_content_length, _KS("0"), true);
			rq->response_connection();
			rq->start_response_body(0);
			goto done;
		}
		if (!KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
			build_status = false;
			rq->response_status(STATUS_CONTENT_PARTIAL);
			rq->response_content_range(rq->sink->data.range, content_length);
		}
		start = rq->sink->data.range->from;
		content_length = rq->ctx.left_read;
	}
	if (!build_obj_header(rq, obj, content_length, build_status)) {
		result = KGL_EUNKNOW;
		goto done;
	}
	if (rq->sink->data.meth == METH_HEAD) {
		rq->ctx.left_read = 0;
		result = KGL_NO_BODY;
		goto done;
	}
	switch (obj->data->i.type) {
#ifdef ENABLE_BIG_OBJECT_206
	case BIG_OBJECT_PROGRESS:
		assert(rq->ctx.obj->data->sbo->can_satisfy(rq->get_range(), rq->ctx.obj) == kgl_satisfy_all);
#endif
	case BIG_OBJECT:
	{
		kfiber_file* file = kgl_open_big_file(rq, rq->ctx.obj, start);
		if (file == NULL) {
			result = KGL_EIO;
			break;
		}
		defer(kfiber_file_close(file););
		int buf_size = conf.io_buffer;
		char* buffer;
		if (rq->ctx.body.f->support_sendfile(rq->ctx.body.ctx)) {
			result = rq->ctx.body.f->sendfile(rq->ctx.body.ctx, file, &rq->ctx.left_read);
			break;
		}
		if (!kasync_file_direct(file, true)) {
			klog(KLOG_ERR, "cann't turn on file direct_on\n");
			result = KGL_EIO;
			break;
		}
		buffer = (char*)aio_alloc_buffer(buf_size);
		if (!buffer) {
			result = KGL_ENO_MEMORY;
			break;
		}
		defer(aio_free_buffer(buffer););
		while (rq->ctx.left_read > 0) {
			int got = kfiber_file_read(file, buffer, (int)(KGL_MIN(rq->ctx.left_read, (INT64)buf_size)));
			if (got <= 0) {
				result = KGL_EIO;
				break;
			}
			assert(got <= rq->ctx.left_read && got <= buf_size);
			rq->ctx.left_read -= got;
			result = rq->ctx.body.f->write(rq->ctx.body.ctx, kfiber_file_adjust(file, buffer), got);
			if (result != KGL_OK) {
				break;
			}
		}
		break;
	}
	case MEMORY_OBJECT:
	{
		kbuf* send_buffer = obj->data->bodys;
		if (rq->ctx.left_read > 0) {
			if (!send_buffer) {
				klog(KLOG_ERR, "obj has no body\n");
				result = KGL_EDATA_FORMAT;
				break;
			}
			kbuf* buf = kbuf_init_read(send_buffer, (int)start, rq->sink->pool);
			assert(buf);
			if (!buf) {
				result = KGL_EUNKNOW;
				break;
			}
			result = kgl_write_buf(&rq->ctx.body, buf, (int)rq->ctx.left_read);
		}
		break;
	}
	default:
		klog(KLOG_ERR, "unknow obj data type=[%d]\n", obj->data->i.type);
		result = KGL_ENOT_SUPPORT;
		break;
	}
done:
	return rq->ctx.body.f->close(rq->ctx.body.ctx, result);
}
/**
* 发送在内存中的object.
*/
KGL_RESULT send_memory_object(KHttpRequest* rq) {
	KHttpObject* obj = rq->ctx.obj;
	if (!kgl_request_match_if_range(rq, obj)) {
		rq->sink->data.range = nullptr;
	}
	return response_cache_object(rq, obj);
}
bool kgl_match_if_range(kgl_precondition_flag flag, kgl_request_range* range, time_t last_modified) {
	if (!range->if_range_entity) {
		return true;
	}
	if (!KBIT_TEST(flag, kgl_precondition_if_range_date)) {
		return false;
	}
	return range->if_range_date == last_modified;
}
bool kgl_match_if_range(kgl_precondition_flag flag, kgl_request_range* range, kgl_len_str_t* etag) {
	if (!range->if_range_entity) {
		return true;
	}
	if (KBIT_TEST(flag, kgl_precondition_if_range_date)) {
		return false;
	}
	return kgl_mem_same(etag->data, etag->len, range->if_range_entity->data, range->if_range_entity->len);
}
bool kgl_request_match_if_range(KHttpRequest* rq, KHttpObject* obj) {
	kgl_request_range* range = rq->sink->data.range;
	if (!range) {
		return false;
	}
	if (!obj->data->etag) {
		return !range->if_range_entity;
	}
	if (obj->data->i.condition_is_time) {
		return kgl_match_if_range(rq->sink->get_precondition_flag(), range, obj->data->last_modified);
	}
	return kgl_match_if_range(rq->sink->get_precondition_flag(), range, obj->data->etag);
}
bool kgl_request_precondition(KHttpRequest* rq, KHttpObject* obj) {
	kgl_precondition_flag flag;
	kgl_precondition* condition = rq->sink->get_precondition(&flag);
	kgl_request_range* range = rq->sink->data.range;
	/**
	* @rfc9110  13.2.2. Precedence of Preconditions
	* cache server ignore step 1 & 2
	*/
	/* step 3 */
	if (KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_none_match && condition) {
		if (!obj->precondition_entity(condition->entity->data, condition->entity->len)) {
			return false;
		}
		return true;
	}
	/* step 4 */
	if (KBIT_TEST(flag, kgl_precondition_mask) == kgl_precondition_if_modified_since && condition) {
		if (!obj->precondition_time(condition->time)) {
			return false;
		}
	}
	return true;
}
/*
obj is not expire
*/
KGL_RESULT send_cache_object(KHttpRequest* rq, KHttpObject* obj) {
	if (kgl_request_precondition(rq, obj)) {
		return send_memory_object(rq);
	}
	return send_http2(rq, obj, STATUS_NOT_MODIFIED);
}
swap_in_result swap_in_object(KHttpRequest* rq, KHttpObject* obj) {
	if (!KBIT_TEST(obj->index.flags, FLAG_IN_MEM)) {
		KMutex* lock = obj->getLock();
		//rq->c->removeRequest(rq,true);
		lock->Lock();
		if (obj->data == NULL) {
			if (cache.IsCleanBlocked()) {
				lock->Unlock();
				return swap_in_failed_clean_blocked;
				//return process_cache_ready_request(rq, obj, swap_in_failed_clean_blocked);
			}
#ifdef ENABLE_DISK_CACHE
			KHttpObjectSwaping* obj_swap = new KHttpObjectSwaping;
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
			KHttpObjectSwaping* os = obj->data->os;
			assert(os);
			return os->wait(lock);
#endif
		}
		lock->Unlock();
	}
	return swap_in_success;
}

KGL_RESULT process_cache_request(KHttpRequest* rq) {
	KHttpObject* obj = rq->ctx.obj;
	if (rq->sink->data.meth != METH_GET && rq->sink->data.meth != METH_HEAD) {
		KBIT_SET(rq->sink->data.flags, RQ_CACHE_HIT);
		if (rq->sink->data.precondition.entity) {
			return send_http2(rq, obj, STATUS_PRECONDITION);
		}
		rq->response_header(_KS("Allow"), _KS("GET, HEAD"));
		return send_http2(rq, obj, STATUS_METH_NOT_ALLOWED);
	}
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
		char* filename = obj->get_filename();
		klog(KLOG_ERR, "obj swap in failed cache file [%s] error=[%d].\n", filename, result);
		free(filename);
#endif
		rq->clean_obj();
		rq->ctx.obj = new KHttpObject(rq);
		if (swap_in_failed_clean_blocked == result) {
			KBIT_SET(rq->ctx.obj->index.flags, FLAG_DEAD);
		}
		if (rq->file) {
			delete rq->file;
			rq->file = NULL;
		}
		assert(!rq->is_source_empty() || KBIT_TEST(rq->sink->get_server_model(), WORK_MODEL_MANAGE) || rq->sink->data.opaque);
		return load_object_from_source(rq);
	}
	}
}
KGL_RESULT fiber_http_start(KHttpRequest* rq) {
	//only if cached
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_ONLY_IF_CACHED)) {
		rq->ctx.obj = find_cache_object(rq, false);
		if (!rq->ctx.obj) {
			return send_error2(rq, 404, "Not in cache");
		}
		return process_cache_request(rq);
	}
	//end purge or only if cached
	if (in_stop_cache(rq)) {
		rq->ctx.obj = new KHttpObject(rq);
		if (!rq->ctx.obj) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
		KBIT_SET(rq->ctx.obj->index.flags, FLAG_DEAD);
	} else {
		rq->ctx.obj = find_cache_object(rq, true);
		if (!rq->ctx.obj) {
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE);
			return send_error2(rq, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
	}
	if (!rq->ctx.obj->in_cache) {
		/* It is a new object */
		if (rq->sink->data.meth != METH_GET) {
			KBIT_SET(rq->ctx.obj->index.flags, FLAG_DEAD);
		}
		return load_object_from_source(rq);
	}
	return process_cache_request(rq);
}
KGL_RESULT prepare_write_body(KHttpRequest* rq, kgl_response_body* body) {
	if (!rq->ctx.body.ctx) {
		if (body) {
			get_default_response_body(rq, body);
		}
		if (!kgl_load_response_body(rq, body)) {
			if (body) {
				body->f->close(body->ctx, KGL_ENOT_PREPARE);
			}
			return KGL_ENOT_PREPARE;
		}
		if (body) {
			rq->ctx.body = *body;
			assert(body->ctx);
		}
	}
	return KGL_OK;
}

KGL_RESULT on_upstream_finished_header(KHttpRequest* rq, kgl_response_body* body) {
	KHttpObject* obj = rq->ctx.obj;
	switch (obj->data->i.status_code) {
	case 0:
		obj->data->i.status_code = STATUS_OK;
		break;
	case STATUS_OK:
	case 206:
		break;
	case 203:
	case 204:
	case 301:
	case 302:
	case 307:
	case 308:
	case 404:
	case 414:
	case 501:
		KBIT_SET(obj->index.flags, OBJ_NOT_OK);
		break;
	default:
		//not cachable status_code.
		KBIT_SET(obj->index.flags, FLAG_DEAD | OBJ_NOT_OK);
		break;
	}
	if (checkResponse(rq, obj) == JUMP_DENY) {
		send_error2(rq, 403, "denied by check response");
		return KGL_EDENIED;
	}
	if (obj->isNoBody(rq)) {
		return KGL_NO_BODY;
	}
	if (obj->data->i.status_code == STATUS_OK) {
		rq->sink->data.range = nullptr;
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (obj->data->i.type == MEMORY_OBJECT) {
		if (rq->sink->data.meth == METH_GET
			&& conf.cache_part
			&& obj_can_disk_cache(rq, obj)
			&& objCanCache(rq, obj)
			&& obj->getTotalContentSize() >= conf.max_cache_size) {
			if (KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_LENGTH | OBJ_NOT_OK) == ANSW_HAS_CONTENT_LENGTH) {
				//obj has content-length and status_code is 200/206.
				return turn_on_bigobject(rq, obj, body);
			}
		}
	}
#endif
	KBIT_CLR(rq->sink->data.flags, RQ_CACHE_HIT);
	rq->dead_old_obj();
	kassert(!kfiber_is_main());
	return prepare_write_body(rq, body);
}

