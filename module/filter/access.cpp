#include <string.h>
#include <sstream>
#include "kstring.h"
#include "ksapi.h"
#include "kmalloc.h"
#include "filter.h"

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
static KGL_RESULT build(kgl_access_build *build_ctx, KF_ACCESS_BUILD_TYPE build_type)
{
	std::stringstream s;
	kgl_footer* mark = (kgl_footer*)build_ctx->module;
	switch (build_type) {
	case KF_ACCESS_BUILD_SHORT:
		build_ctx->write_string(build_ctx->cn, _KS("footer"), 0);
		return KGL_OK;
	case KF_ACCESS_BUILD_HTML: {


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
		build_ctx->write_string(build_ctx->cn, s.str().c_str(), (int)s.str().size(), 0);
		return KGL_OK;
	}
	case KF_ACCESS_BUILD_XML: {
		if (mark && mark->head) {
			s << "head='1'";
		}
		if (mark && mark->replace) {
			s << " replace='1'";
		}
		build_ctx->write_string(build_ctx->cn, s.str().c_str(), (int)s.str().size(), 0);
		return KGL_OK;
	}
	case KF_ACCESS_BUILD_XML_CHARACTER: {
		s << "<![CDATA[";
		if (mark && mark->data) {
			s << mark->data->data;
		}
		s << "]]>";
		build_ctx->write_string(build_ctx->cn, s.str().c_str(), (int)s.str().size(), 0);
		return KGL_OK;
	}
	default:
		return KGL_OK;
	}
}
static KGL_RESULT parse(kgl_access_parse *parse_ctx, KF_ACCESS_PARSE_TYPE parse_type)
{
	kgl_footer* mark = (kgl_footer*)parse_ctx->module;
	switch (parse_type) {
	case KF_ACCESS_PARSE_KV: {
		mark->head = parse_ctx->get_int(parse_ctx->cn, "head") == 1;
		mark->replace = parse_ctx->get_int(parse_ctx->cn, "replace") == 1;
		const char* footer = parse_ctx->get_value(parse_ctx->cn, "footer");
		if (footer && *footer) {
			kstring_release(mark->data);
			mark->data = kstring_from(footer);
		}
		break;
	}
	case KF_ACCESS_PARSE_XML_CHARACTER:
		kstring_release(mark->data);
		mark->data = kstring_from(parse_ctx->get_value(parse_ctx->cn, NULL));
		break;
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
	process,
	NULL
};
void register_access(kgl_dso_version *ver)
{	
	KGL_REGISTER_ACCESS(ver, &footer_model);
}
