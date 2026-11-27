#include <limits.h>
#include "KInputFilter.h"
#include "KMultiPartInputFilter.h"
#include "KHttpRequest.h"
#include "utils.h"
#include "http.h"
#include "KUrlParser.h"
#include "filter.h"
#include "kmalloc.h"
#include "KHttpLib.h"
static int64_t input_filter_get_left(kgl_request_body_ctx* ctx)
{
	KInputFilterContext* if_ctx = (KInputFilterContext*)ctx;
	return if_ctx->body.f->get_left(if_ctx->body.ctx);
}
static int input_filter_read(kgl_request_body_ctx* ctx, char* buf, int len) {
	KInputFilterContext* if_ctx = (KInputFilterContext*)ctx;
	len = if_ctx->body.f->read(if_ctx->body.ctx, buf, len);
	if (len < 0) {
		return len;
	}
	if (if_ctx->match(buf, len, if_ctx->body.f->get_left(if_ctx->body.ctx) == 0)) {
		return KGL_EDENIED;
	}
	return len;
}
static void input_filter_close(kgl_request_body_ctx* ctx) {
	KInputFilterContext* if_ctx = (KInputFilterContext*)ctx;
	if_ctx->body.f->close(if_ctx->body.ctx);
}
static kgl_request_body_function input_body_function = {
	input_filter_get_left,
	input_filter_read,
	input_filter_close
};
void parseUrlParam(char* buf, int len, char** name, int* name_len, char** value, int* value_len)
{
	char* eq = (char*)memchr(buf, '=', len);
	*name_len = len;
	if (eq) {
		*eq = '\0';
		*name_len = (int)(eq - buf);
		eq++;
		*value_len = url_decode(eq, len - (*name_len) - 1, NULL, true);
		*value = eq;
	} else {
		*value = NULL;
		*value_len = 0;
	}
	*name_len = url_decode(buf, *name_len, NULL, true);
	*name = buf;
}
bool KParamInputFilter::match_param(const char* name, int name_len, const char* value, int value_len)
{
	for (auto it = param_list.begin(); it != param_list.end(); ++it) {
		if ((*it)->check(name, name_len, value, value_len)) {
			return true;
		}
	}
	return false;
}
bool KParamInputFilter::match_param_item(char* buf, int len)
{
	char* name, * value;
	int name_len, value_len;
	parseUrlParam(buf, len, &name, &name_len, &value, &value_len);
	return match_param(name, name_len, value, value_len);
}
bool KParamInputFilter::match(KInputFilterContext* rq, const char* str, int len, bool isLast)
{
	/* Append to the parameter left from the last read. The buffer grows by
	 * doubling and only the new bytes are searched for '&': copying and
	 * scanning everything kept on each read made one long parameter cost
	 * quadratic time. */
	if (len < 0 || last_buf_len > INT_MAX - 1 - len) {
		return false;
	}
	int need = last_buf_len + len + 1;
	if (need > last_buf_size) {
		int size = last_buf_size > 0 ? last_buf_size : 4096;
		while (size < need) {
			size = (size > INT_MAX / 2) ? need : size * 2;
		}
		char* nb = (char*)realloc(last_buf, size);
		if (nb == NULL) {
			free(last_buf);
			last_buf = NULL;
			last_buf_len = 0;
			last_buf_size = 0;
			return false;
		}
		last_buf = nb;
		last_buf_size = size;
	}
	if (len > 0) {
		kgl_memcpy(last_buf + last_buf_len, str, len);
	}
	char* scan = last_buf + last_buf_len;
	last_buf_len += len;
	char* end = last_buf + last_buf_len;
	*end = '\0';
	char* hot = last_buf;
	for (;;) {
		char* p = (char*)memchr(scan, '&', end - scan);
		if (p == NULL) {
			break;
		}
		*p = '\0';
		if (match_param_item(hot, (int)(p - hot))) {
			last_buf_len = 0;
			return true;
		}
		hot = scan = p + 1;
	}
	int left = (int)(end - hot);
	if (isLast) {
		auto ret = match_param_item(hot, left);
		last_buf_len = 0;
		return ret;
	}
	if (hot != last_buf) {
		memmove(last_buf, hot, left);
	}
	last_buf_len = left;
	last_buf[left] = '\0';
	return false;
}
void KInputFilterContext::tee_body(kgl_request_body* body) {
	this->body = *body;
	body->ctx = (kgl_request_body_ctx *)this;
	body->f = &input_body_function;
}
bool KInputFilterContext::check_get(KParamFilterHook* hook, KREQUEST rq, kgl_access_context* ctx)
{
	KParamPair* last = NULL;
	if (gParamHeader == NULL) {
		kgl_url* url = NULL;
		DWORD size = sizeof(url);
		if (KGL_OK != ctx->f->get_variable(rq, KGL_VAR_URL_ADDR, NULL, &url, &size)) {
			return false;
		}
		if (!url || !url->param || !*url->param) {
			return false;
		}		
		assert(gParamHeader == NULL);
		gParamCopy = xstrdup(url->param);
		if (gParamCopy == NULL) {
			return false;
		}
		char* hot = gParamCopy;
		for (;;) {
			char* p = strchr(hot, '&');
			if (p == NULL) {
				break;
			}
			*p = '\0';
			KParamPair* pair = new KParamPair;
			pair->next = NULL;
			parseUrlParam(hot, (int)(p - hot), &pair->name, &pair->name_len, &pair->value, &pair->value_len);
			hot = p + 1;
			if (last == NULL) {
				gParamHeader = last = pair;
			} else {
				last->next = pair;
				last = pair;
			}
		}
		KParamPair* pair = new KParamPair;
		pair->next = NULL;
		parseUrlParam(hot, (int)strlen(hot), &pair->name, &pair->name_len, &pair->value, &pair->value_len);
		if (last == NULL) {
			gParamHeader = pair;
		} else {
			last->next = pair;
		}
	}
	last = gParamHeader;
	while (last) {
		if (hook->check(last->name, last->name_len, last->value, last->value_len)) {
			return true;
		}
		last = last->next;
	}
	return false;
}
KInputFilter* KInputFilterContext::get_filter(KREQUEST rq, kgl_access_context* ctx)
{
	if (filter) {
		return filter;
	}
	char content_type_buf[256] = { 0 };
	char* content_type = content_type_buf;
	DWORD size = sizeof(content_type_buf);
	KGL_RESULT result = ctx->f->get_variable(rq, KGL_VAR_CONTENT_TYPE, NULL, content_type, &size);
	kgl_auto_cstr big_content_type;
	if (result == KGL_EINSUFFICIENT_BUFFER && size > sizeof(content_type_buf) && size < 65536) {
		big_content_type.reset((char*)malloc(size));
		if (big_content_type) {
			content_type = big_content_type.get();
			result = ctx->f->get_variable(rq, KGL_VAR_CONTENT_TYPE, NULL, content_type, &size);
		}
	}
	if (result != KGL_OK) {
		filter = new KInputFilter;
		return filter;
	}
	if (strncasecmp(content_type, _KS("application/x-www-form-urlencoded")) == 0) {
		filter = new KParamInputFilter;
	} else if (strncasecmp(content_type, _KS("multipart/form-data")) == 0) {
		filter = new KMultiPartInputFilter(content_type+19,size-19);
	} else {
		filter = new KInputFilter;
	}
	return filter;
}
static KGL_RESULT input_tee_body(kgl_request_body_ctx* ctx, KREQUEST rq, kgl_request_body* body) {
	if (!body) {
		return KGL_OK;
	}
	KInputFilterContext* input_ctx = (KInputFilterContext*)ctx;
	input_ctx->tee_body(body);
	return KGL_OK;
}
static kgl_in_filter input_filter = {
	sizeof(kgl_in_filter),
	0,
	input_tee_body
};
static void input_filter_context_cleanup(void* data) {
	delete (KInputFilterContext*)data;
}
KInputFilterContext* get_input_filter_context(KREQUEST rq, kgl_access_context* ctx) {
	auto* request = static_cast<KHttpRequest*>(rq);
	auto filter_ctx = kgl_cleanup_insert(request->sink->pool, input_filter_context_cleanup);
	if (!filter_ctx) {
		return nullptr;
	}
	KInputFilterContext *fc = (KInputFilterContext *)kgl_cleanup_get_data(filter_ctx);
	if (fc != NULL) {
		return fc;
	}
	fc = new KInputFilterContext;
	kgl_cleanup_set_data(filter_ctx, fc);
	ctx->f->support_function(rq, ctx->cn, KF_REQ_IN_FILTER, &input_filter, (void **)&fc);
	return fc;
}
