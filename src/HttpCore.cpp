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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <map>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include <string>
#include <sstream>
#include "global.h"
#include "log.h"
#include "cache.h"
#include "http.h"
#include "utils.h"
#include "KHttpLib.h"

#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KHttpResponseParser.h"
#include "KHttpObjectHash.h"
#include "KHttpKeyValue.h"
#include "KObjectList.h"
#include "KHttpTransfer.h"
#include "KUpstream.h"
#include "kthread.h"
#include "lang.h"
#include "KRequestQueue.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "kselector.h"
#include "KDirectoryFetchObject.h"
#include "KStaticFetchObject.h"
#include "KCacheFetchObject.h"
#include "KRewriteMarkEx.h"
#include "KUrlParser.h"
#include "KHttpProxyFetchObject.h"
#include "KSimulateRequest.h"
#include "KHttpObjectSwaping.h"
#include "KBufferFetchObject.h"
#include "KGzip.h"
#include "ssl_utils.h"
#include "kfiber.h"
#include "HttpFiber.h"
#include "KFetchBigObject.h"
#include "KBigObjectContext.h"
#include "KDefaultFetchObject.h"
using namespace std;
KGL_RESULT send_auth2(KHttpRequest* rq, KAutoBuffer* body) {
	if (conf.auth_delay > 0 && KBIT_TEST(rq->sink->data.flags, RQ_HAS_PROXY_AUTHORIZATION | RQ_HAS_AUTHORIZATION)) {
		kfiber_msleep(conf.auth_delay * 1000);
		//rq->AddTimer(send_auth_timer, body, conf.auth_delay * 1000);
	}
	uint16_t status_code = AUTH_STATUS_CODE;
	if (rq->auth) {
		status_code = rq->auth->GetStatusCode();
	}
	return send_http2(rq, NULL, status_code, body);
}
static void log_request_error(KHttpRequest* rq, int code, const char* reason) {
	auto url = rq->sink->data.raw_url.getUrl();
	klog(KLOG_WARNING, "request error %s %s %s %d %s\n",
		rq->getClientIp(),
		rq->get_method(),
		url ? url.get() : "BAD_URL",
		code,
		reason);
}

KGL_RESULT send_http2(KHttpRequest* rq, KHttpObject* obj, uint16_t status_code, KAutoBuffer* body) {
	//printf("send_http status=[%d],rq=[%p].\n", status_code, rq);
	assert(!kfiber_is_main());
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	rq->response_status(status_code);
	if (rq->auth) {
		rq->auth->insertHeader(rq);
	}
	rq->response_header(kgl_header_server, conf.serverName, conf.serverNameLength, true);
	timeLock.Lock();
	rq->response_header(kgl_header_date, (char*)cachedDateTime, 29);
	timeLock.Unlock();
	if (body) {
		rq->response_header(kgl_header_content_type, kgl_expand_string("text/html; charset=utf-8"), true);
	}
	if (KBIT_TEST(rq->ctx.filter_flags, RF_X_CACHE)) {
		KStringBuf b;
		if (rq->ctx.cache_hit_part) {
			b.WSTR("HIT-PART from ");
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_CACHE_HIT)) {
			b.WSTR("HIT from ");
		} else {
			b.WSTR("MISS from ");
		}
		b << conf.hostname;
		rq->response_header(kgl_expand_string("X-Cache"), b.buf(), b.size());
	}
	INT64 send_len = 0;
	if (!is_status_code_no_body(status_code)) {
		send_len = (body ? body->getLen() : 0);
		rq->response_content_length(send_len);
	}
	if (obj) {
		if (obj->data) {
			if (status_code == STATUS_NOT_MODIFIED) {
				/**
				RFC2616 10.3.5 304 Not Modified
				*/
				KHttpHeader* header = obj->data->headers;
				while (header) {
					if (kgl_is_attr(header, _KS("Expires")) ||
						kgl_is_attr(header, _KS("Content-Location")) ||
						kgl_is_attr(header, _KS("Cache-Control"))) {
						rq->response_header(header, true);
					}
					header = header->next;
				}
				if (obj->data->etag) {
					if (obj->data->i.condition_is_time) {
						rq->response_last_modified(obj->data->last_modified);
					} else {
						rq->response_header(kgl_header_etag, obj->data->etag->data, (int)obj->data->etag->len, true);
					}
				}
			}
			if (status_code == STATUS_RANGE_NOT_SATISFIABLE && KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_RANGE)) {
				rq->response_content_range(nullptr, obj->index.content_range_length);
			}
		}
		//发送Age头
		if (KBIT_TEST(rq->ctx.filter_flags, RF_AGE) && !KBIT_TEST(obj->index.flags, FLAG_DEAD | ANSW_NO_CACHE)) {
			int current_age = (int)obj->get_current_age(kgl_current_sec);
			if (current_age > 0) {
				rq->response_header(kgl_header_age, current_age);
			}
		}
		obj->ResponseVaryHeader(rq);
	}
	rq->response_connection();
	rq->start_response_body(send_len);
	if (body && rq->sink->data.meth != METH_HEAD) {
		return rq->write_buf(body->getHead(), body->getLen());
	}
	return KGL_OK;
}

/**
* 发送错误信息
*/
KGL_RESULT send_error2(KHttpRequest* rq, int code, const char* reason) {
	log_request_error(rq, code, reason);
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER) || rq->ctx.read_huped) {
		return KGL_EHAS_SEND_HEADER;
	}
	KBIT_SET(rq->sink->data.flags, RQ_IS_ERROR_PAGE);
	if (rq->sink->data.meth == METH_HEAD) {
		return send_http2(rq, rq->ctx.obj, code, NULL);
	}
	KAutoBuffer s(rq->sink->pool);
	assert(rq);
	KString errorPage;
	if (conf.gvm->vhs.getErrorPage(code, errorPage)) {
		if (strncasecmp(errorPage.c_str(), "file://", 7) == 0) {
			errorPage = errorPage.substr(7, errorPage.size() - 7);
		}
		if (!isAbsolutePath(errorPage.c_str())) {
			errorPage = conf.path + errorPage;
		}
		kfiber_file* fp = kfiber_file_open(errorPage.c_str(), fileRead, 0);
		if (fp) {
			INT64 len = kfiber_file_size(fp);
			len = KGL_MIN(len, 32768);
			kbuf* buf = new_pool_kbuf_align(rq->sink->pool, int(len));
			int used = kfiber_file_read(fp, buf->data, (int)len);
			buf->used = used;
			if (used > 2) {
				char* p = (char*)memchr(buf->data, '~', used - 2);
				if (p != NULL) {
					char tmp[5];
					snprintf(tmp, 4, "%03d", code);
					kgl_memcpy(p, tmp, 3);
				}
			}
			s.Append(buf);
			kfiber_file_close(fp);
			return send_http2(rq, NULL, code, &s);
		}
	}
	s << "<html>\n<head>\n";
	s << "	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
	s << "	<title>" << code << "</title>\n";
	s << "</head>\n<body>\n<div id='main'";
#ifndef KANGLE_FREE
	if (*conf.error_url) {
		s << " style='display:none'";
	}
#endif
	s << ">\n<h3>error: " << code;
	s << "</h3><h3>";
	s << reason;
	s << "</h3></p>\n<p>Please check.</p>\n";
	if (*conf.hostname) {
		s << "<div>hostname: " << conf.hostname << "</div>";
	}
	s << "</div>\n";
	if (*conf.error_url) {
		KStringBuf event_id(32);
		if (conf.log_event_id) {
			event_id.add(rq->sink->data.begin_time_msec, INT64_FORMAT_HEX);
			event_id.WSTR("-");
			event_id.add((INT64)rq, INT64_FORMAT_HEX);
		}
		s << "<script language='javascript'>\n\
	var referer = escape(document.referrer);\n\
	var url = escape(document.URL);\n\
	var msg = '" << url_encode(reason).c_str() << "';\n\
    var hostname='" << conf.hostname << "';\n\
	var event_id='";
		s.write_all(event_id.buf(), event_id.size());
		s << "';\n\
	document.write('<scr'+'ipt language=\"javascript\" src=\"";
		s << conf.error_url;
		if (strchr(conf.error_url, '?') == NULL) {
			s << "?";
		} else {
			s << "&";
		}
		s << "code=" << code;
		if (rq->sink->data.opaque) {
			auto svh = kangle::get_virtual_host(rq);
			s << "&vh=" << svh->vh->name.c_str();
		}
		s << "\"></scr' + 'ipt>');\n";
		s << "</script>\n";
	}
	s << "<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n"
		"<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n"
		"<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n";
	s << "</body></html>";
	return send_http2(rq, rq->ctx.obj, code, &s);
}
/**
* 插入via头
*/
void insert_via(KHttpRequest* rq, KWStream& s, char* old_via, size_t len) {
	if (old_via) {
		s.write_all(old_via, len);
		s.write_all(_KS(", "));
	}
	switch (rq->sink->data.http_version) {
	case 0x300:
		s.write_all(_KS("3 "));
		break;
	case 0x200:
		s.write_all(_KS("2 "));
		break;
	case 0x100:
		s.write_all(_KS("1.0 "));
		break;
	default:
		s.write_all(_KS("1.1 "));
		break;
	}
	if (*conf.hostname) {
		s << conf.hostname;
	} else {
		char ip[MAXIPLEN];
		uint16_t port = rq->sink->get_self_ip(ip, sizeof(ip) - 1);
		s << ip;
		s.write_all(_KS(":"));
		s << port;
	}
	s.write_all(_KS("("));
	s.write_all(conf.serverName, conf.serverNameLength);
	s.write_all(_KS(")"));
}

/*************************
* 创建回应http头信息
*************************/
bool build_obj_header(KHttpRequest* rq, KHttpObject* obj, INT64 content_len, bool build_status) {
	assert(!KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER));
	if (likely(build_status)) {
		uint16_t status_code = obj->data->i.status_code;
		if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_RANGED) && rq->sink->data.status_code == STATUS_CONTENT_PARTIAL) {
			//如果请求是url模拟range，则强制转换206的回应为200
			status_code = STATUS_OK;
		}
		rq->response_status(status_code);
	}
	if (KBIT_TEST(obj->index.flags, ANSW_LOCAL_SERVER)) {
		rq->response_header(kgl_header_server, conf.serverName, conf.serverNameLength, true);
		timeLock.Lock();
		rq->response_header(kgl_header_date, (char*)cachedDateTime, 29);
		timeLock.Unlock();
	}
	//bool via_inserted = false;
	//发送附加的头

	KHttpHeader* header = obj->data->headers;
	while (header) {
		rq->response_header(header, true);
		header = header->next;
	}
	//发送Age头
	if (KBIT_TEST(rq->ctx.filter_flags, RF_AGE) && !KBIT_TEST(obj->index.flags, FLAG_DEAD | ANSW_NO_CACHE)) {
		int current_age = (int)obj->get_current_age(kgl_current_sec);
		if (current_age > 0) {
			rq->response_header(kgl_header_age, current_age);
		}
	}
	if (KBIT_TEST(rq->ctx.filter_flags, RF_X_CACHE)) {
		KStringBuf b;
		if (rq->ctx.cache_hit_part) {
			b.WSTR("HIT-PART from ");
		} else if (KBIT_TEST(rq->sink->data.flags, RQ_CACHE_HIT)) {
			b.WSTR("HIT from ");
		} else {
			b.WSTR("MISS from ");
		}
		b << conf.hostname;
		rq->response_header(kgl_expand_string("X-Cache"), b.buf(), b.size());
	}
	if (!KBIT_TEST(obj->index.flags, FLAG_NO_BODY)) {
		/*
		* no body的不发送content-length
		* head method要发送content-length,但不发送内容
		*/
		rq->response_content_length(content_len);
	}
	obj->ResponseVaryHeader(rq);
	if (obj->data->etag) {
		if (obj->data->i.condition_is_time) {
			rq->response_last_modified(obj->data->last_modified);
		} else {
			rq->response_header(kgl_header_etag, obj->data->etag->data, (int)obj->data->etag->len, true);
		}
	}
	rq->response_connection();
	return rq->start_response_body(content_len);
}
KGL_RESULT response_redirect(kgl_output_stream* out, const char* url, size_t url_len, uint16_t code) {
	out->f->write_status(out->ctx, code);
	timeLock.Lock();
	out->f->write_header(out->ctx, kgl_header_server, conf.serverName, conf.serverNameLength);
	out->f->write_header(out->ctx, kgl_header_date, (char*)cachedDateTime, 29);
	timeLock.Unlock();
	out->f->write_header(out->ctx, kgl_header_location, url, url_len);
	return out->f->write_header_finish(out->ctx, 0, nullptr);
}
bool push_redirect_header(KHttpRequest* rq, const char* url, int url_len, int code) {
	if (code == 0) {
		code = STATUS_FOUND;
	}
	assert(!KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER));
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_SEND_HEADER)) {
		return false;
	}
	rq->response_status(code);
	rq->response_header(kgl_header_server, conf.serverName, conf.serverNameLength, true);
	timeLock.Lock();
	rq->response_header(kgl_header_date, (char*)cachedDateTime, 29);
	timeLock.Unlock();
	rq->response_header(kgl_expand_string("Location"), url, url_len);
	rq->response_connection();
	return true;
}


KFetchObject* bindVirtualHost(KHttpRequest* rq, RequestError* error, KApacheHtaccessContext& htresponse) {
	assert(rq->file == NULL);
	//file = new KFileName;
	assert(!rq->has_final_source());
	bool result = false;
	bool redirect_result = false;
	char* indexPath = NULL;
	bool indexFileFindedResult = false;
	auto svh = kangle::get_virtual_host(rq);
	if (svh->vh->closed) {
		error->set(STATUS_SERVICE_UNAVAILABLE, "virtual host is closed");
		return NULL;
	}
	KSafeSource sfo;
	int jump_type = svh->bindFile(rq, rq->ctx.obj, result, htresponse, sfo);
	if (sfo) {
		return sfo.release();
	}
	if (jump_type == JUMP_DENY) {
		//bind错误.如非法url.
		result = false;
		error->set(STATUS_SERVER_ERROR, "cann't bind file or denied by htaccess.");
		return NULL;
	}
	/*
	if (handled || rq->has_final_source()) {
		//请求已经处理,或者数据源已确定.
		return NULL;
	}
	*/
	if (rq->file == NULL) {
		error->set(STATUS_SERVER_ERROR, "cann't bind local file. file is NULL.");
		return NULL;
	}
	if (result && rq->file->isPrevDirectory()) {
		//查找默认首页
		KFileName* newFile = NULL;
		indexFileFindedResult = svh->vh->getIndexFile(rq, rq->file, &newFile, &indexPath);
		if (indexFileFindedResult) {
			delete rq->file;
			rq->file = newFile;
		}
	}
	auto fo = svh->vh->findPathRedirect(rq, rq->file, (indexPath ? indexPath : rq->sink->data.url->path), result, redirect_result);
	if (indexPath) {
		free(indexPath);
	}
	if (fo) {
		//路径映射源确定
		return fo;
	}
	if (result && rq->file->isDirectory()) {
		//文件为目录处理
		if (!rq->file->isPrevDirectory()) {
			//url后面不是以/结尾,重定向处理
			if (rq->sink->data.meth == METH_GET) {
				return new KPrevDirectoryFetchObject;
			} else {
				error->set(STATUS_METH_NOT_ALLOWED, "method not allowed");
				return NULL;
			}
			result = false;
			goto done;
		}
		//默认首页处理
		if (!indexFileFindedResult) {
			//没有查到默认首页
			if (svh->vh->browse) {
				//如果允许浏览
				return new KDirectoryFetchObject;
			}
			error->set(STATUS_FORBIDEN, "You don't have permission to browse.");
			return NULL;
		}
	}
	//按文件扩展名查找扩展映射
	fo = svh->vh->findFileExtRedirect(rq, rq->file, result, redirect_result);
	if (fo) {
		//映射源确定
		return fo;
	}
#if 0
	//查找默认扩展
	fo = svh->vh->findDefaultRedirect(rq, rq->file, result);
	if (fo) {
		return fo;
	}
#endif
	if (result) {
		if (rq->file->getPathInfoLength() > 0) {
			//静态文件不支持path_info
			result = false;
		}
}
done:
	if (!result) {
		if (rq->sink->data.meth == METH_OPTIONS) {
			return new KDefaultFetchObject;
		}
		if (rq->ctx.obj->data->i.status_code == 0) {
			error->set(STATUS_NOT_FOUND, "No such file or directory.");
		}
		return NULL;
	}
	if (fo) {
		return fo;
	}
	return new KStaticFetchObject;
}
char* find_content_type(KHttpRequest* rq, KHttpObject* obj) {
	//处理content-type
	auto svh = kangle::get_virtual_host(rq);
	char* content_type = NULL;
	if (svh) {
		content_type = svh->vh->getMimeType(obj, rq->file->getExt());
	}
	if (content_type == NULL) {
		content_type = conf.gvm->vhs.getMimeType(obj, rq->file->getExt());
	}
	return content_type;
}

bool make_http_env(KHttpRequest* rq, kgl_input_stream* in, KBaseRedirect* brd, KFileName* file, KEnvInterface* env, bool chrooted) {
	size_t skip_length = 0;
	char tmpbuff[50];
	auto svh = kangle::get_virtual_host(rq);
	if (chrooted && svh) {
		skip_length = svh->vh->doc_root.size() - 1;
	}
	KHttpHeader* av = rq->sink->data.get_header();
	while (av) {

		if (KBIT_TEST(rq->sink->data.flags, RQ_HAVE_EXPECT) && kgl_is_attr(av, _KS("Expect"))) {
			goto do_not_insert;
		}
		if (kgl_is_attr(av, _KS("SCHEME"))) {
			goto do_not_insert;
		}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		if (av->name_is_know) {
			assert(av->val_offset == 0);
			env->add_http_header(kgl_header_type_string[av->know_header].value.data, kgl_header_type_string[av->know_header].value.len, av->buf + av->val_offset, av->val_len);
		} else {
#ifdef HTTP_PROXY
			if (kgl_ncasecmp(av->buf, av->name_len, _KS("Proxy-")) == 0) {
				goto do_not_insert;
			}
#endif
			env->add_http_header(av->buf, av->name_len, av->buf + av->val_offset, av->val_len);
			}
	do_not_insert: av = av->next;
		}
	KStringStream host;
	rq->sink->data.url->GetHost(host);
	env->add_http_header(_KS("Host"), host.c_str(), host.size());
	int64_t content_length;
	assert(in);
	if (in) {
		content_length = in->f->body.get_left(in->body_ctx);
	} else {
		content_length = rq->get_left();
	}
	if (rq_has_content_length(rq, content_length)) {
		int val_len = int2string2(content_length, tmpbuff);
		env->add_http_header(_KS("Content-Length"), tmpbuff, val_len);
	}
	kgl_precondition_flag flag;
	kgl_precondition* condition = in->f->get_precondition(in->ctx, &flag);
	if (condition && condition->time > 0) {
		if (KBIT_TEST(flag, kgl_precondition_if_time)) {
			char* end = make_http_time(condition->time, tmpbuff, sizeof(tmpbuff));
			if (KBIT_TEST(flag, kgl_precondition_if_match_unmodified)) {
				env->add_http_header(_KS("If-Unmodified-Since"), (char*)tmpbuff, end - tmpbuff);
			} else {
				env->add_http_header(_KS("If-Modified-Since"), (char*)tmpbuff, end - tmpbuff);
			}
		} else {
			if (KBIT_TEST(flag, kgl_precondition_if_match_unmodified)) {
				env->add_http_header(_KS("If-Match"), condition->entity->data, condition->entity->len);
			} else {
				env->add_http_header(_KS("If-None-Match"), condition->entity->data, condition->entity->len);
			}
		}
	}
	kgl_request_range* range = in->f->get_range(in->ctx);
	if (range) {
		KStringBuf s;
		s << "bytes=";
		if (range->from >= 0) {
			s << range->from << "-";
			if (range->to >= 0) {
				s << range->to;
			}
		} else {
			s << range->from;
		}
		env->add_http_header(_KS("Range"), s.c_str(), s.size());
		if (range->if_range_entity) {
			if (KBIT_TEST(flag, kgl_precondition_if_range_date)) {
				char* end = make_http_time(range->if_range_date, tmpbuff, sizeof(tmpbuff));
				env->add_http_header(_KS("If-Range"), tmpbuff, end - tmpbuff);
			} else {
				env->add_http_header(_KS("If-Range"), range->if_range_entity->data, (int)range->if_range_entity->len);
			}
		}
	}
	timeLock.Lock();
	env->addEnv("SERVER_SOFTWARE", conf.serverName);
	timeLock.Unlock();
	env->addEnv("GATEWAY_INTERFACE", "CGI/1.1");
	env->addEnv("SERVER_NAME", rq->sink->data.url->host);
	env->addEnv("SERVER_PROTOCOL", "HTTP/1.1");
	env->addEnv("REQUEST_METHOD", rq->get_method());
	const char* param = rq->sink->data.raw_url.param;
	if (param == NULL) {
		env->addEnv("REQUEST_URI", rq->sink->data.raw_url.path);
		if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL", rq->sink->data.raw_url.path);
		}
	} else {
		KStringBuf request_uri;
		request_uri << rq->sink->data.raw_url.path << "?" << param;
		env->addEnv("REQUEST_URI", request_uri.c_str());
		if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL", request_uri.c_str());
		}
	}
	if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_REWRITED)) {
		param = rq->sink->data.url->param;
	}
	if (param) {
		env->addEnv("QUERY_STRING", param);
	}
	/*
	SCRIPT_NAME和PATH_INFO区别
	/test.php/a

	SCIPRT_NAME = /test.php
	全PATH_INFO(isapi默认)
	PATH_INFO = /test.php/a
	部分PATH_INFO
	PATH_INFO = /a
	*/
	if (file) {
		unsigned pathInfoLength = file->getPathInfoLength();
		if (file->getIndex()) {
			//有index文件情况下。
			KStringBuf s;
			s << rq->sink->data.url->path << file->getIndex();
			env->addEnv("SCRIPT_NAME", s.c_str());
			if (KBIT_TEST(rq->ctx.filter_flags, RQ_FULL_PATH_INFO)) {
				env->addEnv("PATH_INFO", s.c_str());
			}
		} else {
			if (pathInfoLength > 0) {
				//有path info的情况下
				char* scriptName = (char*)xmalloc(pathInfoLength + 1);
				kgl_memcpy(scriptName, rq->sink->data.url->path, pathInfoLength);
				scriptName[pathInfoLength] = '\0';
				env->addEnv("SCRIPT_NAME", scriptName);
				xfree(scriptName);
				if (!KBIT_TEST(rq->ctx.filter_flags, RQ_FULL_PATH_INFO)) {
					env->addEnv("PATH_INFO", rq->sink->data.url->path + pathInfoLength);
				}
			} else {
				env->addEnv("SCRIPT_NAME", rq->sink->data.url->path);
			}
			if (KBIT_TEST(rq->ctx.filter_flags, RQ_FULL_PATH_INFO)) {
				env->addEnv("PATH_INFO", rq->sink->data.url->path);
			}
		}
		if (skip_length == 0 || skip_length < strlen(file->getName())) {
			if (pathInfoLength > 0) {
				KStringBuf s;
				s << file->getName() + skip_length << rq->sink->data.url->path + pathInfoLength;
				env->addEnv("PATH_TRANSLATED", s.c_str());
			} else {
				env->addEnv("PATH_TRANSLATED", file->getName() + skip_length);
			}
			env->addEnv("SCRIPT_FILENAME", file->getName() + skip_length);
		}
	}

	char ips[MAXIPLEN];
	uint16_t self_port = rq->sink->get_self_ip(ips, sizeof(ips));
	env->addEnv("SERVER_ADDR", ips);
	env->addEnv("SERVER_PORT", self_port);
	env->addEnv("REMOTE_ADDR", rq->getClientIp());
	env->addEnv("REMOTE_PORT", ksocket_addr_port(rq->sink->get_peer_addr()));
	if (svh) {
		env->addEnv("DOCUMENT_ROOT", svh->doc_root + skip_length);
		env->addEnv("VH_NAME", svh->vh->name.c_str());
	}
#ifdef KSOCKET_SSL
	if (KBIT_TEST(rq->sink->data.raw_url.flags, KGL_URL_SSL)) {
		env->addEnv("HTTPS", "ON");
		kssl_session* ssl = rq->sink->get_ssl();
		if (ssl) {
#ifdef SSL_READ_EARLY_DATA_SUCCESS
			if (ssl->in_early) {
				env->addEnv("SSL_EARLY_DATA", "1");
			}
#endif
			make_ssl_env(env, ssl->ssl);
			}
		}
#endif
#ifdef ENABLE_UPSTREAM_PARAM
	if (brd && !brd->params.empty()) {
		for (auto it = brd->params.begin(); it != brd->params.end(); ++it) {
			auto s = KRewriteMarkEx::getString(NULL, (*it).value.c_str(), rq, NULL, NULL);
			if (s) {
				env->add_env((*it).name.c_str(), (*it).name.size(), (const char*)s->c_str(), s->size());
				delete s;
			}
		}
	}
#endif
	return env->addEnvEnd();

	}

int checkResponse(KHttpRequest* rq, KHttpObject* obj) {
	if (KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) || rq->ctx.response_checked || rq->ctx.skip_access) {
		return JUMP_ALLOW;
	}
	rq->ctx.response_checked = 1;
	KSafeSource fo;
	int action = kaccess[RESPONSE]->check(rq, obj, fo);
	if (fo) {
		klog(KLOG_ERR, "response check not support new source\n");
	}
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	if (action == JUMP_ALLOW && rq->sink->data.opaque) {
		action = static_cast<KSubVirtualHost*>(rq->sink->data.opaque)->vh->checkResponse(rq, fo);
		if (fo) {
			klog(KLOG_ERR, "virtualhost response check not support new source\n");
		}
	}
#endif
#endif
	return action;
}
