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
#include "lib.h"

#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KHttpResponseParser.h"
#include "KHttpObjectHash.h"
#include "KHttpKeyValue.h"
#include "KObjectList.h"
#include "KHttpTransfer.h"
#include "KSendable.h"
#include "KUpstream.h"
#include "kthread.h"
#include "lang.h"
#include "KRequestQueue.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "kselector.h"
#include "KDirectoryFetchObject.h"
#include "KStaticFetchObject.h"
#include "KSyncFetchObject.h"
#include "KCacheFetchObject.h"
#include "KRewriteMarkEx.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpProxyFetchObject.h"
#include "KSimulateRequest.h"
#include "KHttpObjectSwaping.h"
#include "KBufferFetchObject.h"
#include "KGzip.h"
#include "ssl_utils.h"
#include "kfiber.h"
#include "HttpFiber.h"
//{{ent
#include "KFetchBigObject.h"
#include "KBigObjectContext.h"
#include "KAntiRefresh.h"
//}}
kgl_str_t kgl_header_type_string[] = {
	{ kgl_expand_string("Unknow") },
	{ kgl_expand_string("Internal") },
	{ kgl_expand_string("Server") },
	{ kgl_expand_string("Date") },
	{ kgl_expand_string("Content-Length")},
	{ kgl_expand_string("Last-Modified")},
};
using namespace std;
bool adjust_range(KHttpRequest *rq,INT64 &len)
{
        //printf("before from=%lld,to=%lld,len=%lld\n",rq->range_from,rq->range_to,len);
	if (rq->range_from >= 0){
		if(rq->range_from >= len) {
			klog(KLOG_ERR,"[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n",rq->getClientIp(),rq->raw_url.host,rq->raw_url.path,rq->range_from,rq->range_to,len);
			return false;
		}
		len-=rq->range_from;
		if (rq->range_to >= 0) {
			len = MIN(rq->range_to - rq->range_from + 1,len);
			if(len<=0){
				klog(KLOG_ERR,"[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n",rq->getClientIp(),rq->raw_url.host,rq->raw_url.path,rq->range_from,rq->range_to,len);
				return false;
			}
		}
	}else if(rq->range_from < 0){
		rq->range_from += len;	
		if(rq->range_from<0){
			rq->range_from = 0;
		}
		len-=rq->range_from;
	}
	rq->range_to = rq->range_from + len - 1;
       	//printf("after from=%lld,to=%lld,len=%lld\n",rq->range_from,rq->range_to,len);
	return true;
}
//compress kbuf
kbuf *deflate_buff(kbuf *in_buf, int level, INT64 &len, bool fast) {
	KBuffer buffer;
	KGzipCompress gzip(conf.gzip_level);
	gzip.connect(&buffer, false);
	gzip.setFast(fast);
	while (in_buf && in_buf->used > 0) {
		if (gzip.write_all(NULL, in_buf->data, in_buf->used)!=STREAM_WRITE_SUCCESS) {
			return NULL;
		}
		in_buf = in_buf->next;
	}
	if (gzip.write_end(NULL, KGL_OK)!=STREAM_WRITE_SUCCESS) {
		return NULL;
	}
	len = buffer.getLen();
	return buffer.stealBuffFast();
}
char * skip_next_line(char *str, int &str_len) {
	int line_pos;
	if (str_len == 0) {
		return NULL;
	}
	char *next_line = (char *) memchr(str, '\n', str_len);
	if (next_line == NULL) {
		//	printf("next line is NULL\n");
		return NULL;
	}
	line_pos = next_line - str + 1;
	str += line_pos;
	str_len -= line_pos;
	return str;
}

KGL_RESULT send_auth2(KHttpRequest *rq, KAutoBuffer *body)
{
	if (conf.auth_delay > 0 && TEST(rq->flags, RQ_HAS_PROXY_AUTHORIZATION | RQ_HAS_AUTHORIZATION)) {
		kfiber_msleep(conf.auth_delay * 1000);
		//rq->AddTimer(send_auth_timer, body, conf.auth_delay * 1000);
	}
	uint16_t status_code = AUTH_STATUS_CODE;
	if (rq->auth) {
		status_code = rq->auth->GetStatusCode();
	}
	return send_http2(rq, NULL, status_code, body);
}
static void log_request_error(KHttpRequest *rq, int code, const char *reason)
{
	char *url = rq->raw_url.getUrl();
	klog(KLOG_WARNING, "request error %s %s %s %d %s\n",
		rq->getClientIp(),
		rq->getMethod(),
		url ? url : "BAD_URL",
		code,
		reason);
	if (url) {
		xfree(url);
	}
}

KGL_RESULT send_http2(KHttpRequest *rq, KHttpObject *obj, uint16_t status_code, KAutoBuffer *body) {
	//printf("send_http status=[%d],rq=[%p].\n", status_code, rq);
	assert(!kfiber_is_main());
	if (TEST(rq->flags, RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	rq->responseStatus(status_code);
	if (rq->auth) {
		rq->auth->insertHeader(rq);
	}
	timeLock.Lock();
	rq->responseHeader(kgl_header_server, conf.serverName, conf.serverNameLength);
	rq->responseHeader(kgl_header_date, (char *)cachedDateTime, 29);
	timeLock.Unlock();
	if (body) {
		rq->responseHeader(kgl_expand_string("Content-Type"), kgl_expand_string("text/html; charset=utf-8"));
	}
	if (TEST(rq->filter_flags, RF_X_CACHE)) {
		KStringBuf b;
		if (rq->ctx->cache_hit_part) {
			b.WSTR("HIT-PART from ");
		} else if (rq->ctx->cache_hit) {
			b.WSTR("HIT from ");
		} else {
			b.WSTR("MISS from ");
		}
		b << conf.hostname;
		rq->responseHeader(kgl_expand_string("X-Cache"), b.getBuf(), b.getSize());
	}
	INT64 send_len = 0;
	if (!is_status_code_no_body(status_code)) {
		send_len = (body ? body->getLen() : 0);
		rq->responseHeader(kgl_expand_string("Content-Length"), (int)send_len);
	}
	if (obj) {
		if (obj->data) {
			/**
			RFC2616 10.3.5 304 Not Modified
			*/
			KHttpHeader *header = obj->data->headers;
			while (header) {
				if (is_attr(header, "Expires") ||
					is_attr(header, "Content-Location") ||
					is_attr(header, "Etag") ||
					is_attr(header, "Last-Modified") ||
					is_attr(header, "Cache-Control") ||
					is_attr(header, "Vary")) {
					rq->responseHeader(header->attr, header->attr_len, header->val, header->val_len);
				}
				header = header->next;
			}
		}
		//发送Age头
		if (TEST(rq->filter_flags, RF_AGE) && !TEST(obj->index.flags, FLAG_DEAD | ANSW_NO_CACHE)) {
			int current_age = (int)obj->getCurrentAge(kgl_current_sec);
			if (current_age > 0) {
				rq->responseHeader(kgl_expand_string("Age"), current_age);
			}
		}
		obj->ResponseVaryHeader(rq);
	}
	rq->responseConnection();
	rq->startResponseBody(send_len);
	if (body) {
		if (!rq->WriteBuff(body->getHead())) {
			return rq->HandleResult(KGL_ESOCKET_BROKEN);
		}
	}
	return KGL_OK;
}

/**
* 发送错误信息
*/
KGL_RESULT send_error2(KHttpRequest *rq, int code, const char *reason)
{
	log_request_error(rq, code, reason);
	if (TEST(rq->flags, RQ_HAS_SEND_HEADER) || rq->ctx->read_huped) {
		return KGL_EHAS_SEND_HEADER;
		/*
		if (!TEST(rq->flags, RQ_SYNC)) {
			return stageEndRequest(rq);
		}
		return kev_err;
		*/
	}
	SET(rq->flags, RQ_IS_ERROR_PAGE);
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		assert(false);
		printf("TODO: bigobject handle error\n");
		return KGL_EUNKNOW;
		//return rq->bo_ctx->handleError(rq, code, reason);
	}
#endif//}}
	if (rq->meth == METH_HEAD) {
		return send_http2(rq, NULL, code, NULL);
	}
	KAutoBuffer s(rq->pool);
	assert(rq);
	std::string errorPage;
	if (conf.gvm->globalVh.getErrorPage(code, errorPage)) {
		if (strncasecmp(errorPage.c_str(), "file://", 7) == 0) {
			errorPage = errorPage.substr(7, errorPage.size() - 7);
		}
		if (!isAbsolutePath(errorPage.c_str())) {
			errorPage = conf.path + errorPage;
		}
		KFile fp;
		if (fp.open(errorPage.c_str(), fileRead)) {
			INT64 len = fp.getFileSize();
			len = MIN(len, 32768);
			kbuf *buf = new_pool_kbuf(rq->pool, int(len));
			int used = fp.read(buf->data, (int)len);
			buf->used = used;
			if (used > 2) {
				char *p = (char *)memchr(buf->data, '~', used - 2);
				if (p != NULL) {
					char tmp[5];
					snprintf(tmp, 4, "%03d", code);
					kgl_memcpy(p, tmp, 3);
				}
			}
			s.Append(buf);
			return send_http2(rq, NULL, code, &s);
		}
	}
	//const char *status = KHttpKeyValue::getStatus(code);
	s << "<html>\n<head>\n";
	s << "	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
	s << "	<title>" << code << "</title>\n";
	s << "</head>\n<body>\n<div id='main'";
	//{{ent
#ifndef KANGLE_FREE
	if (*conf.error_url) {
		s << " style='display:none'";
	}
#endif//}}
	s << ">\n<h3>error: " << code;
	s << "</h3><h3>";
	s << reason;
	s << "</h3></p>\n<p>Please check.</p>\n";
	if (*conf.hostname) {
		s << "<div>hostname: " << conf.hostname << "</div>";
	}
	//s << "<hr>\n";
	//s << "<div id='pb'>";
	//{{ent
	//if (*conf.error_url == '\0')//}}
	//	s << "Generated by <a href='https://www.cdnbest.com/?code=" << code << "' target=_blank>" << PROGRAM_NAME << "/" << VERSION << "</a>.\n";
	//s << "</div>\n";
	s << "</div>\n";
	//{{ent
#ifndef KANGLE_FREE
	if (*conf.error_url) {
		KStringBuf event_id(32);
		if (conf.log_event_id) {
			event_id.add(rq->begin_time_msec, INT64_FORMAT_HEX);
			event_id.WSTR("-");
			event_id.add((INT64)rq, INT64_FORMAT_HEX);
		}
		s << "<script language='javascript'>\n\
	var referer = escape(document.referrer);\n\
	var url = escape(document.URL);\n\
	var msg = '" << url_encode(reason).c_str() << "';\n\
    var hostname='" << conf.hostname << "';\n\
	var event_id='";
		s.write_all(NULL, event_id.getBuf(), event_id.getSize());
		s << "';\n\
	document.write('<scr'+'ipt language=\"javascript\" src=\"";
		if (*conf.error_url) {
			s << conf.error_url;
			if (strchr(conf.error_url, '?') == NULL) {
				s << "?";
			} else {
				s << "&";
			}
		} else {
			s << "https://error.kangleweb.net/?";
		}
		s << "code=" << code;
		if (rq->svh) {
			s << "&vh=" << rq->svh->vh->name.c_str();
		}
		s << "\"></scr' + 'ipt>');\n";
		s << "</script>\n";
	}
#endif//}}
	s << "<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n"
		"<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n"
		"<!-- padding for ie -->\n<!-- padding for ie -->\n<!-- padding for ie -->\n";
	s << "</body></html>";
	return send_http2(rq, NULL, code, &s);
}
/**
* 插入via头
*/
void insert_via(KHttpRequest *rq, KWStream &s, char *old_via) {
	s << "Via: ";
	if (old_via) {
		s << old_via << ",";
	}
	s << (int)rq->http_major << "." << (int)rq->http_minor << " ";
	if (*conf.hostname) {
		s << conf.hostname;
	} else {
		sockaddr_i addr;
		rq->sink->GetSelfAddr(&addr);
		char ip[MAXIPLEN];
		ksocket_sockaddr_ip(&addr,ip,sizeof(ip));
		s << ip << ":" << ksocket_addr_port(&addr);
	}
	s << "(";
	timeLock.Lock();
	s.write_all(conf.serverName,conf.serverNameLength);
	timeLock.Unlock();
	s << ")\r\n";
}
/*************************
* 创建回应http头信息
*************************/
bool build_obj_header(KHttpRequest *rq, KHttpObject *obj,INT64 content_len, INT64 &start, INT64 &send_len) {
	start = 0;
	send_len = content_len;
	assert(!TEST(rq->flags,RQ_HAS_SEND_HEADER));
	//SET(rq->flags,RQ_HAS_SEND_HEADER);
	if (obj->data->status_code == 0) {
		obj->data->status_code = STATUS_OK;
	}
	bool build_first = true;
	bool send_obj_header = true;
	if (TEST(rq->flags, RQ_HAVE_RANGE)
		&& obj->data->status_code == STATUS_OK 
		&& content_len>0) {
		send_len = content_len;
		if (!adjust_range(rq,send_len)) {			
			build_first = false;
			send_obj_header = false;
			rq->responseStatus(416);
			rq->responseHeader(kgl_expand_string("Content-Length"), kgl_expand_string("0"));
			rq->range_from = -1;
			start = -1;
			send_len = 0;
		} else {
			if (!TEST(rq->raw_url.flags,KGL_URL_RANGED)) {				
				build_first = false;
				rq->responseStatus(STATUS_CONTENT_PARTIAL);
				KStringBuf s;
				s.WSTR("bytes ");
				s.add(rq->range_from,INT64_FORMAT);
				s.WSTR("-");
				s.add(rq->range_to, INT64_FORMAT);
				s.WSTR("/");
				s.add(content_len, INT64_FORMAT);
				rq->responseHeader(kgl_expand_string("Content-Range"),s.getBuf(),s.getSize());
			}
			start = rq->range_from;
			content_len = send_len;
		}
	}
	if (build_first) {
		uint16_t status_code = obj->data->status_code;
		if (TEST(rq->raw_url.flags,KGL_URL_RANGED) && rq->status_code==STATUS_CONTENT_PARTIAL) {
			//如果请求是url模拟range，则强制转换206的回应为200
			status_code = STATUS_OK;
		}
		rq->responseStatus(status_code);
	}
	if (TEST(obj->index.flags,ANSW_LOCAL_SERVER)) {		
		timeLock.Lock();
		rq->responseHeader(kgl_header_server, conf.serverName, conf.serverNameLength);
		rq->responseHeader(kgl_header_date,(char *)cachedDateTime,29);
		timeLock.Unlock();
	}
	//bool via_inserted = false;
	//发送附加的头
	if (likely(send_obj_header)) {
		KHttpHeader *header = obj->data->headers;
		while (header) {
			rq->responseHeader(header->attr, header->attr_len, header->val, header->val_len);
			header = header->next;
		}
		//发送Age头
		if (TEST(rq->filter_flags, RF_AGE) && !TEST(obj->index.flags, FLAG_DEAD | ANSW_NO_CACHE)) {
			int current_age = (int)obj->getCurrentAge(kgl_current_sec);
			if (current_age > 0) {
				rq->responseHeader(kgl_expand_string("Age"), current_age);
			}
		}
		if (TEST(rq->filter_flags, RF_X_CACHE)) {
			KStringBuf b;
			if (rq->ctx->cache_hit_part) {
				b.WSTR("HIT-PART from ");
			} else if (rq->ctx->cache_hit) {
				b.WSTR("HIT from ");
			} else {
				b.WSTR("MISS from ");
			}
			b << conf.hostname;
			rq->responseHeader(kgl_expand_string("X-Cache"), b.getBuf(), b.getSize());
		}
		if (!TEST(obj->index.flags, FLAG_NO_BODY)) {
			/*
			* no body的不发送content-length
			* head method要发送content-length,但不发送内容
			*/
			rq->responseContentLength(content_len);
		}		
		obj->ResponseVaryHeader(rq);
	}
	rq->responseConnection();	
	rq->startResponseBody(send_len);
	return true;
}

void prepare_write_stream(KHttpRequest *rq)
{
	kassert(rq->tr == NULL);
	kassert(rq->ctx->st == NULL);
	rq->tr = new KHttpTransfer(rq, rq->ctx->obj);
	rq->ctx->st = rq->tr;// makeWriteStream(rq, rq->ctx->obj, rq->tr, autoDelete);
	kassert(!rq->IsFetchObjectEmpty());
}

bool push_redirect_header(KHttpRequest *rq, const char *url,int url_len,int code) {
	if (code==0) {
		code = STATUS_FOUND;
	}
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return false;
	}
	rq->responseStatus(code);
	timeLock.Lock();
	rq->responseHeader(kgl_header_server, conf.serverName, conf.serverNameLength);
	rq->responseHeader(kgl_header_date, (char *)cachedDateTime, 29);
	timeLock.Unlock();
	rq->responseHeader(kgl_expand_string("Location"),url,url_len);
	rq->responseHeader(kgl_expand_string("Content-Length"), 0);
	rq->responseConnection();
	return true;
}

kbuf *build_memory_obj_header(KHttpRequest *rq, KHttpObject *obj,INT64 &start,INT64 &send_len)
{	
	INT64 content_len = obj->index.content_length;
	if (!obj->in_cache && !TEST(obj->index.flags, ANSW_HAS_CONTENT_LENGTH)) {
		content_len = -1;
	}
	send_len = content_len;
	start = 0;
	build_obj_header(rq, obj, content_len, start, send_len);
	if (TEST(obj->index.flags, FLAG_NO_BODY) || rq->meth == METH_HEAD || start == -1) {
		send_len = -1;
		/*
		 * do not need send body
		 */
		return NULL;
	}
	return obj->data->bodys;
}

KFetchObject *bindVirtualHost(KHttpRequest *rq,RequestError *error,KAccess **htresponse,bool &handled) {
	assert(rq->file==NULL);
	//file = new KFileName;
	assert(!rq->HasFinalFetchObject());
	KFetchObject *redirect = NULL;
	bool result = false;
	bool redirect_result = false;
	char *indexPath = NULL;
	bool indexFileFindedResult = false;
	if(rq->svh->vh->closed){
		error->set(STATUS_SERVICE_UNAVAILABLE,"virtual host is closed");
		return NULL;
	}
	if (!rq->svh->bindFile(rq,rq->ctx->obj,result,htresponse,handled)) {
		//bind错误.如非法url.
		result = false;
		error->set(STATUS_SERVER_ERROR, "cann't bind file.");
		return NULL;
	}
	if (handled || rq->HasFinalFetchObject()) {
		//请求已经处理,或者数据源已确定.
		return NULL;
	}
	if (rq->file==NULL) {
		error->set(STATUS_SERVER_ERROR,"cann't bind local file. file is NULL.");
		return NULL;
	}
	if (result && rq->file->isPrevDirectory()) {
		//查找默认首页
		KFileName *newFile = NULL;
		indexFileFindedResult = rq->svh->vh->getIndexFile(rq,rq->file,&newFile,&indexPath);
		if (indexFileFindedResult) {
			delete rq->file;
			rq->file = newFile;
		}
	}
	redirect = rq->svh->vh->findPathRedirect(rq, rq->file,(indexPath?indexPath:rq->url->path), result,redirect_result);
	if (indexPath) {
		free(indexPath);
	}
	if (redirect) {
		//路径映射源确定
		return redirect;
	}
	if (redirect_result) {
		//路径映射源为空,但映射成功,意思就是用默认处理
		goto done;
	}
	if (result && rq->file->isDirectory()) {
		//文件为目录处理
		if (!rq->file->isPrevDirectory()) {
			//url后面不是以/结尾,重定向处理
			if(rq->meth == METH_GET){
				return new KPrevDirectoryFetchObject;
			}else{
				error->set(STATUS_METH_NOT_ALLOWED,"method not allowed");
				return NULL;
			}
			result = false;
			goto done;
		}
		//默认首页处理
		if (!indexFileFindedResult) {
			//没有查到默认首页
			if (rq->svh->vh->browse) {
				//如果允许浏览
				return new KDirectoryFetchObject;
			}
			error->set(STATUS_FORBIDEN,"You don't have permission to browse.");
			return NULL;
		}
	}
	//按文件扩展名查找扩展映射
	redirect = rq->svh->vh->findFileExtRedirect(rq, rq->file, result,redirect_result);
	if (redirect) {
		//映射源确定
		return redirect;
	}
	if(redirect_result){
		//映射源为空,但映射成功,意思就是用默认处理
		goto done;
	}
	//查找默认扩展
	redirect = rq->svh->vh->findDefaultRedirect(rq,rq->file,result);
	if(redirect){
		return redirect;
	}else if(result){
		if(rq->file->getPathInfoLength()>0){
			//静态文件不支持path_info
			result = false;
		}
	}
done:
	if (!result) {
		if(rq->ctx->obj->data->status_code==0){
			error->set(STATUS_NOT_FOUND, "No such file or directory.");
		}
		return NULL;
	} else if(redirect==NULL) {		
		redirect = new KStaticFetchObject;
	}
	return redirect;
}

inline int attr_tolower(const char p) {
	if (p=='-') {
		return '_';
	}
	return tolower(p);
}
int attr_casecmp(const char *s1,const char *s2)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;
	int result;
	if (p1 == p2)
		return 0;

	while ((result = attr_tolower (*p1) - attr_tolower(*p2++)) == 0)
		if (*p1++ == '\0')
			break;
	return result;
}
bool is_val(KHttpHeader *av, const char *val, int val_len)
{
	if (av->val_len != val_len) {
		return false;
	}
	return strncasecmp(av->val, val, val_len) == 0;
}
bool is_attr(KHttpHeader *av, const char *attr) {
	if (!av || !av->attr || !attr)
		return false;
	return attr_casecmp(av->attr, attr) == 0;
}
bool is_attr(KHttpHeader *av, const char *attr,int attr_len)
{
	assert(av && av->attr && attr);
	return attr_casecmp(av->attr, attr) == 0;
}
bool parse_url(const char *src, KUrl *url) {
	const char *ss, *se, *sx;
	//memset(url, 0, sizeof(KUrl));
	int p_len;
	if (*src == '/') {/* this is 'GET /path HTTP/1.x' request */
		sx = src;
		goto only_path;
	}
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	if (memcmp(ss, "://", 3)) {
		return false;
	}
	p_len = ss - src;
	if (p_len == 4 && strncasecmp(src, "http", p_len) == 0) {
		CLR(url->flags,KGL_URL_ORIG_SSL);
		url->port = 80;
	} else if (p_len == 5 && strncasecmp(src, "https", p_len) == 0) {
		SET(url->flags, KGL_URL_ORIG_SSL);
		url->port = 443;
	}
	//host start
	ss += 3;
	sx = strchr(ss, '/');
	if (sx == NULL) {
		return false;
	}
	p_len = 0;
	if(*ss == '['){
		ss++;
		se = strchr(ss,']');
		SET(url->flags, KGL_URL_IPV6);
		if(se && se < sx) {
			p_len = se - ss;
			se = strchr(se+1,':');
			if(se && se<sx){
				url->port = atoi(se + 1);
			}
		}
	}else{
		se = strchr(ss, ':');
		if(se && se<sx){
			p_len = se - ss;
			url->port = atoi(se + 1);
		}
	}
	if(p_len == 0){
		p_len = sx - ss;
	}
	url->host = (char *) malloc(p_len + 1);
	kgl_memcpy(url->host, ss, p_len);
	url->host[p_len] = 0;
	only_path: const char *sp = strchr(sx, '?');
	int path_len;
	if (sp) {
		char *param = strdup(sp+1);		
		if (*param) {
			url->param = param;
		} else {
			free(param);
		}
		path_len = sp - sx;
	} else {
		path_len = strlen(sx);
	}
	url->path = (char *) xmalloc(path_len+1);
	url->path[path_len] = '\0';
	kgl_memcpy(url->path, sx, path_len);
	return true;
}
char * find_content_type(KHttpRequest *rq,KHttpObject *obj)
{
	//处理content-type
	char *content_type = rq->svh->vh->getMimeType(obj,rq->file->getExt());
	if (content_type==NULL) {
		content_type = conf.gvm->globalVh.getMimeType(obj,rq->file->getExt());
	}
	return content_type;
	/*
	if (content_type==NULL) {
		return false;
	}
	gate->f->push_unknow_header(gate, rq, kgl_expand_string("Content-Type"), content_type, (hlen_t)strlen(content_type));
	xfree(content_type);
	//obj->insertHttpHeader2(strdup("Content-Type"),sizeof("Content-Type")-1, content_type,strlen(content_type));
	return true;
	*/
}

bool make_http_env(KHttpRequest *rq, kgl_input_stream *gate, KBaseRedirect *brd,time_t lastModified,KFileName *file,KEnvInterface *env, bool chrooted) {
	size_t skip_length = 0;
	char tmpbuff[50];
	if (chrooted && rq->svh) {
		skip_length = rq->svh->vh->doc_root.size() - 1;
	}
	KHttpHeader *av = rq->GetHeader();
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif
		if (TEST(rq->flags,RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
		if (is_attr(av,"SCHEME")) {
			goto do_not_insert;
		}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		env->addHttpHeader(av->attr, av->val);
		do_not_insert: av = av->next;
	}
	KStringBuf host;
	rq->url->GetHost(host);
	env->addHttpHeader((char *)"Host", host.getString());
	int64_t content_length;
	if (gate) {
		content_length = gate->f->get_read_left(gate, rq);
	} else {
		content_length = rq->content_length;
	}
	if (rq_has_content_length(rq,content_length)) {
		env->addHttpHeader((char *)"Content-Length",(char *)int2string(content_length,tmpbuff));
	}
	if (lastModified != 0) {
		mk1123time(lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			env->addHttpHeader((char *)"If-Range", (char *)tmpbuff);
		} else {
			env->addHttpHeader((char *)"If-Modified-Since", (char *)tmpbuff);
		}
	} else if (rq->ctx->if_none_match) {
		if (rq->ctx->mt == modified_if_range_etag) {
			env->addHttpHeader((char *)"If-Range",rq->ctx->if_none_match->data);
		} else {
			env->addHttpHeader((char *)"If-None-Match",rq->ctx->if_none_match->data);
		}
	}
	timeLock.Lock();
	env->addEnv("SERVER_SOFTWARE", conf.serverName);
	timeLock.Unlock();
	env->addEnv("GATEWAY_INTERFACE", "CGI/1.1");
	env->addEnv("SERVER_NAME", rq->url->host);
	env->addEnv("SERVER_PROTOCOL", "HTTP/1.1");
	env->addEnv("REQUEST_METHOD", rq->getMethod());
	const char *param = rq->raw_url.param;
	if (param==NULL) {
		env->addEnv("REQUEST_URI",rq->raw_url.path);
		if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL",rq->raw_url.path);
		}
	} else {
		KStringBuf request_uri;
		request_uri << rq->raw_url.path << "?" << param;
		env->addEnv("REQUEST_URI",request_uri.getString());
		if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL",request_uri.getString());
		}
	}
	if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
		param = rq->url->param;
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
		if(file->getIndex()) {
			//有index文件情况下。
			KStringBuf s;
			s << rq->url->path << file->getIndex();
			env->addEnv("SCRIPT_NAME", s.getString());
			if(TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
				env->addEnv("PATH_INFO",s.getString());
			}
		} else {		
			if(pathInfoLength>0){
				//有path info的情况下
				char *scriptName = (char *)xmalloc(pathInfoLength+1);
				kgl_memcpy(scriptName,rq->url->path,pathInfoLength);
				scriptName[pathInfoLength] = '\0';
				env->addEnv("SCRIPT_NAME",scriptName);
				xfree(scriptName);
				if(!TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
					env->addEnv("PATH_INFO",rq->url->path + pathInfoLength);
				}
			}else{
				env->addEnv("SCRIPT_NAME", rq->url->path);
			}
			if(TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
				env->addEnv("PATH_INFO",rq->url->path);
			}
		}
		if (skip_length < file->getNameLen()) {
			if(pathInfoLength>0){
				KStringBuf s;
				s << file->getName() + skip_length << rq->url->path + pathInfoLength;
				env->addEnv("PATH_TRANSLATED",s.getString());
			}else{
				env->addEnv("PATH_TRANSLATED", file->getName() + skip_length);
			}
			env->addEnv("SCRIPT_FILENAME", file->getName() + skip_length);
		}
	}
	sockaddr_i self_addr;
	rq->sink->GetSelfAddr(&self_addr);
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
	env->addEnv("SERVER_ADDR", ips);
	env->addEnv("SERVER_PORT", rq->raw_url.port);
	env->addEnv("REMOTE_ADDR", rq->getClientIp());	
	env->addEnv("REMOTE_PORT", ksocket_addr_port(rq->sink->GetAddr()));
	if (rq->svh) {
		env->addEnv("DOCUMENT_ROOT", rq->svh->doc_root + skip_length);
		env->addEnv("VH_NAME", rq->svh->vh->name.c_str());
	}
#ifdef KSOCKET_SSL
	if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {
		env->addEnv("HTTPS", "ON");
		kssl_session *ssl = rq->sink->GetSSL();
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
	//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
	if (brd && brd->params.size()>0) {
		std::list<KParamItem>::iterator it;
		for (it=brd->params.begin();it!=brd->params.end();it++) {
			KStringBuf *s = KRewriteMarkEx::getString(NULL,(*it).value.c_str(),rq,NULL,NULL);
			if (s) {
				env->addEnv((*it).name.c_str(),(const char *)s->getString());
				delete s;
			}
		}
	}
#endif//}}
	return env->addEnvEnd();

}

int checkResponse(KHttpRequest *rq,KHttpObject *obj)
{
	if (TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) || rq->ctx->response_checked || rq->ctx->skip_access) {
		return JUMP_ALLOW;
	}
	rq->ctx->response_checked = 1;
	int action = kaccess[RESPONSE].check(rq,obj);
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	if (action == JUMP_ALLOW && rq->svh) {
		action = rq->svh->vh->checkResponse(rq);
	}
#endif
#endif
	if (action == JUMP_DENY) {
		KMutex *lock = obj->getLock();
		lock->Lock();
		if (obj->refs == 1) {
			destroy_kbuf(obj->data->bodys);
			obj->data->bodys = NULL;
			set_obj_size(obj, 0);
		}
		lock->Unlock();
	}
	return action;
}
