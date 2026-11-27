#include "ksapi.h"
#include "KInputFilter.h"
#include "KReg.h"

/*
 * The compiled filename rule an upload is checked with. It is used while
 * the body is read, after the access lock is gone, so it never changes
 * once built: a reload of a named mark builds a new one.
 */
class KPostFileRule : public KFileFilterHook
{
public:
	bool check_content(const char* str, int len) override {
		return false;
	}
	bool check_name(const char* name, int name_len, const char* filename, int filename_len, KHttpHeader* header) override  {
		bool result = false;
		if (filename && filename_len > 0) {
			result = (reg_filename.match(filename, filename_len, 0) > 0);
		}
		if (result) {
			katom_inc((void*)&hit);
		}
		return result;
	}
	KReg reg_filename;
	volatile int32_t hit = 0;
};
class KPostFileMark
{
public:
	KPostFileMark() {
		icase = 0;
		rule = new KPostFileRule;
	}
	~KPostFileMark() {
		rule->release();
	}
	void build_short(KWStream& s) {
		if (icase) {
			s << "[I]";
		}
		s << rule->reg_filename.getModel();
		s << " hit:" << (int)rule->hit;
		return;
	}
	static void build_html(KPostFileMark* urlAcl, KWStream& s) {
		s << "filename(regex):<textarea name='filename' rows='1'>";
		if (urlAcl) {
			s << urlAcl->rule->reg_filename.getModel();
		}
		s << "</textarea>";
		s << "<input type='checkbox' value='1' name='icase' ";
		if (urlAcl && urlAcl->icase) {
			s << "checked";
		}
		s << ">ignore case";
		return;
	}
	void parse(kgl_access_parse_config* parse_ctx) {
		icase = !!parse_ctx->body->get_int(parse_ctx->cn, "icase");
		//revers = !!parse_ctx->body->get_int(parse_ctx->cn, "revers");
		setFilename(parse_ctx->body->get_value(parse_ctx->cn, "filename"));
	}
	/* a reference the caller releases */
	KPostFileRule* get_rule() {
		return static_cast<KPostFileRule*>(rule->add_ref());
	}
private:
	KPostFileRule* rule;
	int icase;
	void setFilename(const char* value) {
		int flag = (icase ? KGL_PCRE_CASELESS : 0);
		KPostFileRule* rule = new KPostFileRule;
		rule->hit = this->rule->hit;
		rule->reg_filename.setModel(value, flag);
		this->rule->release();
		this->rule = rule;
	}
};
static void* create_access_ctx() {
	KPostFileMark* param = new KPostFileMark;
	return param;
}
static void free_access_ctx(void* ctx) {
	KPostFileMark* param = (KPostFileMark*)ctx;
	delete param;
}
static KGL_RESULT build(kgl_access_build* build_ctx, uint32_t build_type) {
	KPostFileMark* mark = (KPostFileMark*)build_ctx->module;
	KBuilderWriter builder(build_ctx);
	try {
		switch (build_type) {
		case KF_ACCESS_BUILD_SHORT:
			mark->build_short(builder);
			return KGL_OK;
		case KF_ACCESS_BUILD_HTML:
			KPostFileMark::build_html(mark, builder);
			return KGL_OK;
		default:
			return KGL_ENOT_SUPPORT;
		}
	} catch (...) {
		return KGL_EUNKNOW;
	}
}
static KGL_RESULT parse(kgl_access_parse_config* parse_ctx) {
	KPostFileMark* mark = (KPostFileMark*)parse_ctx->module;
	mark->parse(parse_ctx);
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context* ctx, DWORD notify) {
	KPostFileMark* mark = (KPostFileMark*)ctx->module;
	auto filter_ctx = get_input_filter_context(rq, ctx);
	if (!filter_ctx) {
		return KF_STATUS_REQ_FALSE;
	}
	KFileFilter rule(mark->get_rule());
	filter_ctx->get_filter(rq, ctx)->register_file(rule.get());
	return KF_STATUS_REQ_FALSE;
}
kgl_access kgl_post_file_model = {
	sizeof(kgl_access),
	KF_NOTIFY_REQUEST_MARK,
	"post_file",
	create_access_ctx,
	free_access_ctx,
	build,
	parse,
	NULL,
	process
};
