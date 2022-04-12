#include <stdlib.h>
#include "KHttpResponseParser.h"
#include "lib.h"
#include "http.h"
#include "KHttpFieldValue.h"
#include "time_utils.h"
#include "kmalloc.h"
kgl_header_result KHttpResponseParser::InternalParseHeader(KHttpRequest *rq, KHttpObject *obj,const char *attr, int attr_len, const char *val, int *val_len, bool request_line)
{

	if (!strcasecmp(attr, "Etag")) {
		KBIT_SET(obj->index.flags, OBJ_HAS_ETAG);
		return kgl_header_insert_begin;
	}
	if (!strcasecmp(attr, "Content-Range")) {
		const char *p = strchr(val, '/');
		if (p) {
			rq->ctx->content_range_length = string2int(p + 1);
			KBIT_SET(obj->index.flags, ANSW_HAS_CONTENT_RANGE);
		}
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Content-length")) {
		obj->index.content_length = string2int(val);
		KBIT_SET(obj->index.flags, ANSW_HAS_CONTENT_LENGTH);
		//KBIT_CLR(obj->index.flags, ANSW_CHUNKED);
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Content-Type")) {
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Date")) {
		serverDate = parse1123time(val);
		//KBIT_SET(obj->index.flags,OBJ_HAS_DATE);
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Last-Modified")) {
		obj->index.last_modified = parse1123time(val);
		if (obj->index.last_modified > 0) {
			obj->index.flags |= ANSW_LAST_MODIFIED;
		}
		return kgl_header_success;
	}
	if (strncasecmp(attr, "Set-Cookie", sizeof("Set-Cookie") - 1) == 0) {
		if (!KBIT_TEST(obj->index.flags, OBJ_IS_STATIC2)) {
			obj->index.flags |= ANSW_NO_CACHE;
		}
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Pragma")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-cache")) {
				obj->index.flags |= ANSW_NO_CACHE;
				break;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
#ifdef ENABLE_STATIC_ENGINE
			if (!KBIT_TEST(obj->index.flags, OBJ_IS_STATIC2)) {
#endif
				if (field.is("no-store")) {
					obj->index.flags |= ANSW_NO_CACHE;
				} else if (field.is("no-cache")) {
					obj->index.flags |= ANSW_NO_CACHE;
				} else if (field.is("private")) {
					obj->index.flags |= ANSW_NO_CACHE;
				}
#ifdef ENABLE_STATIC_ENGINE
			}
#endif
#ifdef ENABLE_FORCE_CACHE
			if (field.is("static")) {
				//通过http header强制缓存
				obj->force_cache();
			} else
#endif
				if (field.is("public")) {
					KBIT_CLR(obj->index.flags, ANSW_NO_CACHE);
				} else if (field.is("max-age=", (int *)&obj->index.max_age)) {
					obj->index.flags |= ANSW_HAS_MAX_AGE;
				} else if (field.is("s-maxage=", (int *)&obj->index.max_age)) {
					obj->index.flags |= ANSW_HAS_MAX_AGE;
				} else if (field.is("must-revalidate")) {
					obj->index.flags |= OBJ_MUST_REVALIDATE;
				}
		} while (field.next());
#ifdef ENABLE_FORCE_CACHE
		if (KBIT_TEST(obj->index.flags, OBJ_IS_STATIC2)) {
			return kgl_header_no_insert;
		}
#endif
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Vary")) {
		if (obj->AddVary(rq, val, *val_len)) {
			return kgl_header_no_insert;
		}
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Age")) {
		age = atoi(val);
		return kgl_header_no_insert;
	}
	if (*attr == 'x' || *attr == 'X') {
		if (!KBIT_TEST(rq->filter_flags, RF_NO_X_SENDFILE) &&
			(strcasecmp(attr, "X-Accel-Redirect") == 0 || strcasecmp(attr, "X-Proxy-Redirect") == 0)) {
			KBIT_SET(obj->index.flags, ANSW_XSENDFILE);
			return kgl_header_insert_begin;
		}
		if (strcasecmp(attr, "X-No-Buffer") == 0) {
			KBIT_SET(rq->filter_flags, RF_NO_BUFFER);
			return kgl_header_no_insert;
		}
		if (strcasecmp(attr, "X-Gzip") == 0) {
			obj->need_compress = 1;
			return kgl_header_no_insert;
		}
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		/*
		if (strcasecmp(val, "chunked") == 0) {
			if (!KBIT_TEST(obj->index.flags, ANSW_HAS_CONTENT_LENGTH)) {
				obj->index.flags |= ANSW_CHUNKED;
			}
			return kgl_header_no_insert;
		}
		*/
		kassert(false);
		return kgl_header_no_insert;
	}
	if (strcasecmp(attr, "Content-Encoding") == 0) {
		if (strcasecmp(val, "none") == 0) {
			return kgl_header_no_insert;
		}
		if (strcasecmp(val, "gzip") == 0) {
			obj->uk.url->set_content_encoding(KGL_ENCODING_GZIP);
		} else	if (strcasecmp(val, "deflate") == 0) {
			obj->uk.url->set_content_encoding(KGL_ENCODING_DEFLATE);
		} else if (strcasecmp(val, "compress") == 0) {
			obj->uk.url->set_content_encoding(KGL_ENCODING_COMPRESS);
		} else if (strcasecmp(val, "br") == 0) {
			obj->uk.url->set_content_encoding(KGL_ENCODING_BR);
		} else if (strcasecmp(val, "identity") == 0) {
			obj->uk.url->accept_encoding = (u_char)~0;
		} else if (*val) {
			//不明content-encoding不能缓存
			KBIT_SET(obj->index.flags, FLAG_DEAD);
			obj->uk.url->set_content_encoding(KGL_ENCODING_UNKNOW);
		}
		return kgl_header_success;
	}
	if (!KBIT_TEST(obj->index.flags, ANSW_HAS_EXPIRES) &&
		!strcasecmp(attr, "Expires")) {
		KBIT_SET(obj->index.flags, ANSW_HAS_EXPIRES);
		expireDate = parse1123time(val);
		return kgl_header_success;
	}
	//{{ent
#ifdef HTTP_PROXY
	if (strcasecmp(attr, "Proxy-Authenticate") == 0) {
		return kgl_header_no_insert;
	}
#endif
	//}}
	return kgl_header_success;
}
bool KHttpResponseParser::ParseHeader(KHttpRequest *rq, const char *attr, int attr_len, const char *val, int val_len, bool request_line)
{
	//printf("%s: %s\n", attr, val);
	switch (InternalParseHeader(rq, rq->ctx->obj, attr, attr_len, val, &val_len, request_line)) {
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
void KHttpResponseParser::EndParse(KHttpRequest *rq)
{
	/*
 * see rfc2616
 * 没有 Last-Modified 我们不缓存.
 * 但如果有 expires or max-age  除外
 */
	if (!KBIT_TEST(rq->ctx->obj->index.flags, ANSW_LAST_MODIFIED | OBJ_HAS_ETAG)) {
		if (!KBIT_TEST(rq->ctx->obj->index.flags, ANSW_HAS_MAX_AGE | ANSW_HAS_EXPIRES)) {
			KBIT_SET(rq->ctx->obj->index.flags, ANSW_NO_CACHE);
		}
	}
	if (!KBIT_TEST(rq->ctx->obj->index.flags, ANSW_NO_CACHE)) {
		if (serverDate == 0) {
			serverDate = kgl_current_sec;
		}
		time_t responseTime = kgl_current_sec;
		/*
		 * the age calculation algorithm see rfc2616 sec 13
		 */
		unsigned apparent_age = 0;
		if (responseTime > serverDate) {
			apparent_age = (unsigned)(responseTime - serverDate);
		}
		unsigned corrected_received_age = MAX(apparent_age, age);

		unsigned response_delay = (unsigned)(responseTime - rq->begin_time_msec / 1000);
		unsigned corrected_initial_age = corrected_received_age	+ response_delay;
		unsigned resident_time = (unsigned)(kgl_current_sec - responseTime);
		age = corrected_initial_age + resident_time;
		if (!KBIT_TEST(rq->ctx->obj->index.flags, ANSW_HAS_MAX_AGE)
			&& KBIT_TEST(rq->ctx->obj->index.flags, ANSW_HAS_EXPIRES)) {
			rq->ctx->obj->index.max_age = (unsigned)(expireDate - serverDate) - age;
		}
	}
	CommitHeaders(rq);
	//first_body_time = kgl_current_sec;
}
void KHttpResponseParser::CommitHeaders(KHttpRequest *rq)
{
	if (last) {
		last->next = rq->ctx->obj->data->headers;
		rq->ctx->obj->data->headers = StealHeader();
	}
}