#include "ksapi.h"
#include "KInputFilter.h"
#include "KReg.h"
#include "KStringBuf.h"

class KParamCountContext : public KParamFilterHook
{
public:
	KParamCountContext(int max_count) {
		this->max_count = max_count;
		count = 0;
	}
	bool check(const char* name, int name_len, const char* value, int value_len) override {
		count++;
		return count > max_count;
	}
private:
	int count;
	int max_count;
};
class KParamCountMark
{
public:
	KParamCountMark() {
		get = true;
		post = true;
		max_count = 1000;
	}
	~KParamCountMark() {

	}
	void build_short(KWStream& s) {
		s << max_count;
		return;
	}
	static void build_html(KParamCountMark *urlAcl, KWStream& s) {
		s << "max count:<input name='max_count' value='";
		if (urlAcl) {
			s << urlAcl->max_count;
		}
		s << "'><br>";
		s << "<input type='checkbox' value='1' name='get' ";
		if (urlAcl == NULL || urlAcl->get) {
			s << "checked";
		}
		s << ">get";
		s << "<input type='checkbox' value='1' name='post' ";
		if (urlAcl == NULL || urlAcl->post) {
			s << "checked";
		}
		s << ">post";
		return;
	}
	void parse(kgl_access_parse_config* parse_ctx) {
		max_count = (int)parse_ctx->body->get_int(parse_ctx->cn,"max_count");
		get = !!parse_ctx->body->get_int(parse_ctx->cn, "get");
		post = !!parse_ctx->body->get_int(parse_ctx->cn, "post");
	}
	KSharedObj<KParamCountContext> new_context() {
		return KSharedObj<KParamCountContext>(new KParamCountContext(max_count));
	}
	int max_count;
	bool get;
	bool post;
};
static void* create_access_ctx() {
	KParamCountMark* param = new KParamCountMark;
	return param;
}
static void free_access_ctx(void* ctx) {
	KParamCountMark* param = (KParamCountMark*)ctx;
	delete param;
}
static KGL_RESULT build(kgl_access_build* build_ctx, uint32_t build_type) {
	KParamCountMark* mark = (KParamCountMark*)build_ctx->module;
	KBuilderWriter builder(build_ctx);
	try {
		switch (build_type) {
		case KF_ACCESS_BUILD_SHORT:
			mark->build_short(builder);
			return KGL_OK;
		case KF_ACCESS_BUILD_HTML:
			KParamCountMark::build_html(mark, builder);
			return KGL_OK;
		default:
			return KGL_ENOT_SUPPORT;
		}
	} catch (...) {
		return KGL_EUNKNOW;
	}
}
static KGL_RESULT parse(kgl_access_parse_config* parse_ctx) {
	KParamCountMark* mark = (KParamCountMark*)parse_ctx->module;
	mark->parse(parse_ctx);
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context* ctx, DWORD notify) {
	KParamCountMark* mark = (KParamCountMark*)ctx->module;
	auto filter_ctx = get_input_filter_context(rq, ctx);
	if (!filter_ctx) {
		return KF_STATUS_REQ_TRUE;
	}
	auto context = mark->new_context();
	if (mark->get) {
		if (filter_ctx->check_get(context.get(), rq, ctx)) {
			return KF_STATUS_REQ_FINISHED;
		}
	}
	if (mark->post) {
		filter_ctx->get_filter(rq, ctx)->register_param(context.get());
	}
	return KF_STATUS_REQ_TRUE;
}
kgl_access kgl_param_count_model = {
	sizeof(kgl_access),
	KF_NOTIFY_REQUEST_MARK,
	"param_count",
	create_access_ctx,
	free_access_ctx,
	build,
	parse,
	NULL,
	process
};