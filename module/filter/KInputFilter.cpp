#include "KInputFilter.h"
#include "KMultiPartInputFilter.h"
#include "utils.h"
#include "http.h"
#include "KUrlParser.h"
#include "filter.h"
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
		return -1;
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
	char* eq = strchr(buf, '=');
	*name_len = len;
	if (eq) {
		*eq = '\0';
		*name_len = (int)(eq - buf);
		eq++;
		*value_len = url_decode(eq, len - (*name_len) - 1, NULL, true);
		*value = eq;
	} else {
		*value = NULL;
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
	char* buf;
	if (last_buf) {
		int new_len = last_buf_len + len;
		buf = (char*)malloc(new_len + 1);
		kgl_memcpy(buf, last_buf, last_buf_len);
		kgl_memcpy(buf + last_buf_len, str, len);
		len = new_len;
		free(last_buf);
		last_buf = NULL;
	} else {
		buf = (char*)malloc(len + 1);
		kgl_memcpy(buf, str, len);
	}
	buf[len] = '\0';
	char* hot = buf;
	for (;;) {
		char* p = strchr(hot, '&');
		if (p == NULL) {
			break;
		}
		*p = '\0';
		if (match_param_item(hot, (int)(p - hot))) {
			free(buf);
			return true;
		}
		hot = p + 1;
	}
	last_buf_len = (int)strlen(hot);
	if (isLast) {
		auto ret = match_param_item(hot, last_buf_len);
		free(buf);
		return ret;
	}
	last_buf = strdup(hot);
	free(buf);
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
		char* hot = url->param;
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
	char content_type[256] = { 0 };
	DWORD size = sizeof(content_type);
	if (ctx->f->get_variable(rq, KGL_VAR_CONTENT_TYPE, NULL, content_type, &size) != KGL_OK) {
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
	auto pool = dso_version->pool->get_request_pool(rq);
	auto filter_ctx = dso_version->pool->cleanup_insert(pool, input_filter_context_cleanup);
	KInputFilterContext *fc = (KInputFilterContext *)dso_version->pool->cleanup_get_data(filter_ctx);
	if (fc != NULL) {
		return fc;
	}
	fc = new KInputFilterContext;
	dso_version->pool->cleanup_set_data(filter_ctx, fc);
	ctx->f->support_function(rq, ctx->cn, KF_REQ_IN_FILTER, &input_filter, (void **)&fc);
	return fc;
}
