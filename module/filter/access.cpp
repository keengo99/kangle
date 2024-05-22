#include <string.h>
#include <sstream>
#include "kstring.h"
#include "ksapi.h"
#include "kmalloc.h"
#include "filter.h"

extern kgl_access kgl_param_model;
extern kgl_access kgl_param_count_model;
extern kgl_access kgl_post_file_model;

static int notify_type = KF_NOTIFY_RESPONSE_MARK;
static void *create_access_ctx()
{
	kgl_footer *footer =  new kgl_footer;
	memset(footer, 0, sizeof(kgl_footer));
	return footer;
}
static void free_access_ctx(void *ctx)
{
	kgl_footer* footer = (kgl_footer*)ctx;
	kstring_release(footer->data);
	delete footer;
}
static KGL_RESULT build(kgl_access_build *build_ctx, uint32_t build_type)
{
	std::stringstream s;
	kgl_footer* mark = (kgl_footer*)build_ctx->module;
	switch (build_type) {
	case KF_ACCESS_BUILD_SHORT:
		build_ctx->write_string(build_ctx->cn, _KS("***"), 0);
		return KGL_OK;
	case KF_ACCESS_BUILD_HTML:
		s << "<textarea name='footer'>";
		if (mark && mark->data) {
			s << mark->data->data;
		}
		s << "</textarea>";
		s << "<input type=checkbox name=head value='1' ";
		if (mark && mark->head) {
			s << "checked";
		}
		s << ">add to head";
		s << "<input type=checkbox name=replace value='1' ";
		if (mark && mark->replace) {
			s << "checked";
		}
		s << ">replace";
		build_ctx->write_string(build_ctx->cn, s.str().c_str(), (int)s.str().length(), 0);
		return KGL_OK;
	default:
		return KGL_ENOT_SUPPORT;
	}
}
static KGL_RESULT parse(kgl_access_parse_config *parse_ctx)
{
	kgl_footer* mark = (kgl_footer*)parse_ctx->module;	
	mark->head = parse_ctx->body->get_int(parse_ctx->cn, "head") == 1;
	mark->replace = parse_ctx->body->get_int(parse_ctx->cn, "replace") == 1;
	const char* footer = parse_ctx->body->get_value(parse_ctx->cn, "footer");
	if (footer && *footer) {
		kstring_release(mark->data);
		mark->data = kstring_from(footer);
	}
	const char* text = parse_ctx->body->get_text(parse_ctx->cn);
	if (*text) {
		kstring_release(mark->data);
		mark->data = kstring_from(text);
	}
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context *ctx, DWORD notify)
{
	kgl_footer* mark = (kgl_footer*)ctx->module;
	kassert(notify == notify_type);
	kgl_filter_footer* footer = new kgl_filter_footer;
	footer->footer = *mark;
	kstring_refs(mark->data);
	register_footer(rq, ctx, footer);
	return KF_STATUS_REQ_TRUE;
}
static kgl_access footer_model = {
	sizeof(kgl_access),
	notify_type,
	"footer",
	create_access_ctx,
	free_access_ctx,
	build,
	parse,
	NULL,
	process,
	NULL
};
void register_access(kgl_dso_version *ver)
{	
	KGL_REGISTER_ACCESS(ver, &footer_model);
	KGL_REGISTER_ACCESS(ver, &kgl_param_model);
	KGL_REGISTER_ACCESS(ver, &kgl_param_count_model);
	KGL_REGISTER_ACCESS(ver, &kgl_post_file_model);
}
