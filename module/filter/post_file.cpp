#include "ksapi.h"
#include "KInputFilter.h"
#include "KReg.h"

class KPostFileMark : public KFileFilterHook
{
public:
	KPostFileMark() {
		icase = 0;
		hit = 0;
	}
	bool check_content(const char* str, int len) override {
		return false;
	}
	bool check_name(const char* name, int name_len, const char* filename, int filename_len, KHttpHeader* header) override  {
		bool result = false;
		if (filename && filename_len > 0) {
			result = (reg_filename.match(filename, filename_len, 0) > 0);
		}
		if (result) {
			hit++;
		}
		return result;
	}
	void build_short(KWStream& s) {
		if (icase) {
			s << "[I]";
		}
		s << reg_filename.getModel();
		s << " hit:" << hit;
		return;
	}
	static void build_html(KPostFileMark* urlAcl, KWStream& s) {
		s << "filename(regex):<textarea name='filename' rows='1'>";
		if (urlAcl) {
			s << urlAcl->reg_filename.getModel();
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
private:
	KReg reg_filename;
	int icase;
	bool setFilename(const char* value) {
		int flag = (icase ? KGL_PCRE_CASELESS : 0);
		return this->reg_filename.setModel(value, flag);
	}
	int hit;
};
static void* create_access_ctx() {
	KPostFileMark* param = new KPostFileMark;
	return param;
}
static void free_access_ctx(void* ctx) {
	KPostFileMark* param = (KPostFileMark*)ctx;
	param->release();
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
		return KF_STATUS_REQ_TRUE;
	}
	filter_ctx->get_filter(rq, ctx)->register_file(mark);
	return KF_STATUS_REQ_TRUE;
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