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

#include "KHttpLib.h"
#include "ksocket.h"

#include "log.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KTable.h"
#include "utils.h"
#include "KHttpResponseParser.h"
#include "KVirtualHost.h"
#include "KEnvInterface.h"
#include "KPipeStream.h"
#include "kselector.h"
#include "KPathRedirect.h"
#include "KPushGate.h"

enum {
	LOAD_HEAD_SUCCESS, LOAD_HEAD_FAILED, LOAD_HEAD_RETRY
};
inline bool is_internal_header(KHttpHeader *av) {
	return av->name_is_internal;
	//return av->type == kgl_header_internal;
}
class KHttpEnv;
void log_access(KHttpRequest *rq);
void upstream_sign_request(KHttpRequest *rq, KHttpEnv *s);
KFetchObject *bindVirtualHost(KHttpRequest *rq, RequestError *error, KApacheHtaccessContext &htresponse);
//void prepare_write_stream(KHttpRequest *rq);
KGL_RESULT load_object_from_source(KHttpRequest* rq);
KGL_RESULT send_auth2(KHttpRequest *rq, KAutoBuffer *body = NULL);
char *find_content_type(KHttpRequest *rq,KHttpObject *obj);
bool build_obj_header(KHttpRequest* rq, KHttpObject* obj, INT64 content_len, bool build_status = true);
bool push_redirect_header(KHttpRequest *rq,const char *url,int url_len,int code);
KGL_RESULT response_redirect(kgl_output_stream* out, const char* url, size_t url_len, uint16_t code=302);
void insert_via(KHttpRequest *rq, KWStream &s, char *old_via = NULL,size_t len = 0);
bool make_http_env(KHttpRequest *rq,kgl_input_stream *gate, KBaseRedirect *rd,KFileName *file,KEnvInterface *env, bool chrooted=false);
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
//检查obj是否过期
inline bool check_object_expiration(KHttpRequest *rq,KHttpObject *obj) {
	assert(obj);
	if (KBIT_TEST(obj->index.flags, OBJ_IS_GUEST) && !KBIT_TEST(rq->ctx.filter_flags, RF_GUEST)) {
		//是游客缓存，但该用户不是游客
		return true;
	}
	if (KBIT_TEST(obj->index.flags,OBJ_MUST_REVALIDATE)) {
		//有must-revalidate,则每次都要从源上验证
		return true;
	}
	if (KBIT_TEST(rq->sink->data.flags, RQ_HAS_NO_CACHE)) {
		return true;
	}
	assert(obj->data);
	if (obj->index.last_verified == 0) {
		return true;
	}
	unsigned freshness_lifetime;
	if (KBIT_TEST(obj->index.flags ,ANSW_HAS_MAX_AGE|ANSW_HAS_EXPIRES)){
		freshness_lifetime = obj->data->i.max_age;
	} else {
		freshness_lifetime = conf.refresh_time;
	}

	if (KBIT_TEST(rq->ctx.filter_flags,RF_DOUBLE_CACHE_EXPIRE)) {
		//双倍过期时间，并清除强制刷新
		freshness_lifetime = freshness_lifetime<<1;
		KBIT_CLR(rq->sink->data.flags,RQ_HAS_NO_CACHE);
	}
	//debug("current_age=%d,refreshness_lifetime=%d\n",current_age,freshness_lifetime);
	unsigned current_age = obj->get_current_age(kgl_current_sec);
	if (current_age >= freshness_lifetime){
		return true;
	}

	return false;
}
inline bool is_cache_object_expired(KHttpRequest *rq, KHttpObject *obj)
{
	if (check_object_expiration(rq, obj)) {
#ifdef ENABLE_STATIC_ENGINE
		//如果是静态化的页面,则要重新生成过
		KBIT_CLR(obj->index.flags, OBJ_IS_STATIC2);
#endif
		goto revalidate;
	}
	return false;
revalidate:
#ifdef ENABLE_FORCE_CACHE
	if (KBIT_TEST(obj->index.flags, OBJ_IS_STATIC2)) {
		return false;
	}
#endif
	assert(obj && rq->ctx.old_obj == NULL);
	rq->ctx.old_obj = obj;
	rq->ctx.obj = new KHttpObject(rq);
	return true;
}
KGL_RESULT process_check_cache_expire(KHttpRequest* rq, KHttpObject* obj);
inline bool in_stop_cache(KHttpRequest *rq) {
	if (KBIT_TEST(rq->ctx.filter_flags, RF_NO_CACHE)) {
		return true;
	}
	return false;
}
KGL_RESULT process_request(KHttpRequest* rq);
KGL_RESULT process_request_stream(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out);
#endif
