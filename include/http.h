/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef HTTP_H_SADFLKJASDLFKJASDLFKJ234234234
#define HTTP_H_SADFLKJASDLFKJASDLFKJ234234234
#include <string>
#include <vector>
#include <stdlib.h>
#include "global.h"

#include "lib.h"
#include "ksocket.h"

#include "log.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KTable.h"
#include "utils.h"
#include "KHttpResponseParser.h"
#include "KVirtualHost.h"
#include "KSendable.h"
#include "KEnvInterface.h"
#include "KPipeStream.h"
#include "kselector.h"
#include "KPathRedirect.h"
#include "KPushGate.h"

enum {
	LOAD_HEAD_SUCCESS, LOAD_HEAD_FAILED, LOAD_HEAD_RETRY
};
inline bool is_internal_header(KHttpHeader *av) {
	return av->type == kgl_header_internal;
}
class KHttpEnv;
void log_access(KHttpRequest *rq);
void upstream_sign_request(KHttpRequest *rq, KHttpEnv *s);
bool is_val(KHttpHeader *av, const char *val, int val_len);
bool is_attr(KHttpHeader *av, const char *attr);
int attr_casecmp(const char *s1,const char *s2);

char *url_encode(const char *s, size_t len, size_t *new_length);
std::string url_encode(const char *s, size_t len = 0);
bool parse_url(const char *src, KUrl *url);

KFetchObject *bindVirtualHost(KHttpRequest *rq, RequestError *error, KAccess **htresponse, bool &handled);

void prepare_write_stream(KHttpRequest *rq);
KGL_RESULT load_object(KHttpRequest* rq);
StreamState send_buff(KSendable *socket, kbuf *buf , INT64 &start,INT64 &send_len);
KGL_RESULT send_auth2(KHttpRequest *rq, KAutoBuffer *body = NULL);
char *find_content_type(KHttpRequest *rq,KHttpObject *obj);
bool build_obj_header(KHttpRequest *rq, KHttpObject *obj,INT64 content_len, INT64 &start, INT64 &send_len);
bool push_redirect_header(KHttpRequest *rq,const char *url,int url_len,int code);
void insert_via(KHttpRequest *rq, KWStream &s, char *old_via = NULL);
bool make_http_env(KHttpRequest *rq,kgl_input_stream *gate, KBaseRedirect *rd,time_t lastModified,KFileName *file,KEnvInterface *env, bool chrooted=false);
kbuf *build_memory_obj_header(KHttpRequest *rq, KHttpObject *obj, INT64 &start, INT64 &send_len);
bool stored_obj(KHttpObject *obj,int list_state);
bool stored_obj(KHttpRequest *rq, KHttpObject *obj,KHttpObject *old_obj);
void set_obj_size(KHttpObject *obj, INT64 content_length);
int checkResponse(KHttpRequest *rq,KHttpObject *obj);
KGL_RESULT send_cache_object(KHttpRequest* rq, KHttpObject* obj);


inline bool rq_has_content_length(KHttpRequest *rq, int64_t content_length) {
	if (content_length > 0) {
		return true;
	}
	if (content_length == 0 && KBIT_TEST(rq->sink->data.flags, RQ_HAS_CONTENT_LEN)) {
		return true;
	}
	return false;
}
//检查obj是否过期1
inline bool check_object_expiration(KHttpRequest *rq,KHttpObject *obj) {
	assert(obj);
	if (KBIT_TEST(obj->index.flags, OBJ_IS_GUEST) && !KBIT_TEST(rq->filter_flags, RF_GUEST)) {
		//是游客缓存，但该用户不是游客
		return true;
	}
	if (KBIT_TEST(obj->index.flags,OBJ_MUST_REVALIDATE)) {
		//有must-revalidate,则每次都要从源上验证
		return true;
	}
	if (obj->index.last_verified == 0) {
		return true;
	}
	unsigned freshness_lifetime;
	if (KBIT_TEST(obj->index.flags ,ANSW_HAS_MAX_AGE|ANSW_HAS_EXPIRES)){
		freshness_lifetime = obj->index.max_age;
	} else {
		freshness_lifetime = conf.refresh_time;
	}
	if (KBIT_TEST(rq->filter_flags,RF_DOUBLE_CACHE_EXPIRE)) {
		//双倍过期时间，并清除强制刷新
		freshness_lifetime = freshness_lifetime<<1;
		KBIT_CLR(rq->sink->data.flags,RQ_HAS_NO_CACHE);
	}
	//debug("current_age=%d,refreshness_lifetime=%d\n",current_age,freshness_lifetime);
	unsigned current_age = obj->getCurrentAge(kgl_current_sec);
	if (current_age >= freshness_lifetime){
		return true;
	}
	return false;
}
inline bool is_cache_object_expired(KHttpRequest *rq, KHttpObject *obj)
{
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (KBIT_TEST(obj->index.flags, FLAG_BIG_OBJECT_PROGRESS)) {
		/**
		* 未完物件，特殊处理。或产生if-range请求头会验证是否修改。
		*
		*/
		return false;
	}
#endif//}}
	if (check_object_expiration(rq, obj)) {
#ifdef ENABLE_STATIC_ENGINE
		//如果是静态化的页面,则要重新生成过
		KBIT_CLR(obj->index.flags, OBJ_IS_STATIC2);
#endif
		goto revalidate;
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_NO_CACHE)) {
		goto revalidate;
	}
	return false;
revalidate:
#ifdef ENABLE_FORCE_CACHE
	if (KBIT_TEST(obj->index.flags, OBJ_IS_STATIC2)) {
		return false;
	}
#endif
	assert(obj && rq->ctx->old_obj == NULL);
	rq->ctx->old_obj = obj;
	rq->ctx->obj = new KHttpObject(rq);
	rq->ctx->new_object = true;
	return true;
}
inline KGL_RESULT process_check_cache_expire(KHttpRequest* rq, KHttpObject* obj)
{
	if (is_cache_object_expired(rq, obj)) {
		return load_object(rq);
	}
	return send_cache_object(rq, obj);
}
inline bool in_stop_cache(KHttpRequest *rq) {
	if (KBIT_TEST(rq->filter_flags, RF_NO_CACHE)) {
		return true;
	}
	if (rq->sink->data.meth == METH_GET || rq->sink->data.meth == METH_HEAD) {
		return false;
	}
	return true;
}
KGL_RESULT process_request(KHttpRequest* rq, KFetchObject* fo);
#endif
