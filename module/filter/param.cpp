#include "ksapi.h"
#include "KInputFilter.h"
#include "KReg.h"
#include "KStringBuf.h"

class KParamMark : public KParamFilterHook
{
public:
	KParamMark() {
		icase = 0;
		name = NULL;
		value = NULL;
		get = true;
		post = true;
		hit = 0;
	}

	const char* getName() {
		return "param";
	}
	void build_short(KWStream& s) {
		if (icase) {
			s << "[I]";
		}
		if (name) {
			s << "name:" << name << " ";
		}
		if (value) {
			s << "value:" << value;
		}
		s << " hit:" << hit;
		return;
	}
	static void build_html(KParamMark* m,KWStream &s) {
		s << "param name:(regex)<textarea name='name' rows='1'>";
		if (m && m->name) {
			s << m->name;
		}
		s << "</textarea><br>";
		s << "param value:(regex)<textarea name='value' rows='1'>";
		if (m && m->value) {
			s << m->value;
		}
		s << "</textarea><br>";
		s << "charset:<input name='charset' value='";
		if (m) {
			s << m->charset;
		}
		s << "'><br>";
		s << "<input type='checkbox' value='1' name='icase' ";
		if (m && m->icase) {
			s << "checked";
		}
		s << ">ignore case";
		s << "<input type='checkbox' value='1' name='get' ";
		if (m == NULL || m->get) {
			s << "checked";
		}
		s << ">get";
		s << "<input type='checkbox' value='1' name='post' ";
		if (m == NULL || m->post) {
			s << "checked";
		}
		s << ">post";
		return;
	}
	void parse(kgl_access_parse_config* parse_ctx) {		
		icase = !!parse_ctx->body->get_int(parse_ctx->cn, "icase");
		charset = parse_ctx->body->get_value(parse_ctx->cn, "charset");// attribute["charset"];
		get = !!parse_ctx->body->get_int(parse_ctx->cn, "get");
		post = !!parse_ctx->body->get_int(parse_ctx->cn, "post");
		set_value(parse_ctx->body->get_value(parse_ctx->cn, "name"), parse_ctx->body->get_value(parse_ctx->cn, "value"));
	}
	bool check(const char* name, int name_len, const char* value, int value_len) override {
		if (this->name) {
			if (name == NULL) {
				return false;
			}
			if (this->reg_name.match(name, name_len, 0) <= 0) {
				return false;
			}
			if (this->value == NULL) {
				hit++;
				return true;
			}
		}
		if (this->value) {
			if (value == NULL) {
				return false;
			}
			if (this->reg_value.match(value, value_len, 0) > 0) {
				hit++;
				return true;
			}
		}
		return false;
	}
	bool get;
	bool post;
	bool icase;
	int hit;
protected:
	~KParamMark() {
		if (name) {
			free(name);
		}
		if (value) {
			free(value);
		}
	}
private:
	KString charset;
	char* name;
	char* value;
	KReg reg_name;
	KReg reg_value;
	bool set_value(const char* name, const char* value) {
		int flag = (icase ? KGL_PCRE_CASELESS : 0);
		bool result = false;
		if (this->name) {
			free(this->name);
			this->name = NULL;
		}
		if (this->value) {
			free(this->value);
			this->value = NULL;
		}
		if (value && *value) {
			this->value = strdup(value);
		}
		if (name && *name) {
			this->name = strdup(name);
		}
		if (charset.size() > 0 && strcasecmp(charset.c_str(), "utf-8") != 0) {
#if 0
			char* str = NULL;
			if (this->value) {
				str = utf82charset(this->value, strlen(this->value), charset.c_str());
				if (str) {
					result = this->reg_value.setModel(str, flag);
					free(str);
				}
			}
			if (this->name) {
				str = utf82charset(this->name, strlen(this->name), charset.c_str());
				if (str) {
					result = this->reg_name.setModel(str, flag);
					free(str);
				}
			}
#endif
		} else {
#ifdef PCRE_UTF8
			flag |= PCRE_UTF8;
#endif
			if (this->value) {
				result = this->reg_value.setModel(this->value, flag);
			}
			if (this->name) {
				this->reg_name.setModel(this->name, flag);
			}
		}
		return result;
	}
};
static void* create_access_ctx() {
	KParamMark* param = new KParamMark;
	return param;
}
static void free_access_ctx(void* ctx) {
	KParamMark* param = (KParamMark*)ctx;
	param->release();
}
static KGL_RESULT build(kgl_access_build* build_ctx, uint32_t build_type) {
	KParamMark* mark = (KParamMark*)build_ctx->module;
	KBuilderWriter builder(build_ctx);
	try {
		switch (build_type) {
		case KF_ACCESS_BUILD_SHORT:
			mark->build_short(builder);
			return KGL_OK;
		case KF_ACCESS_BUILD_HTML:
			KParamMark::build_html(mark, builder);
			return KGL_OK;
		default:
			return KGL_ENOT_SUPPORT;
		}
	} catch (...) {
		return KGL_EUNKNOW;
	}
}
static KGL_RESULT parse(kgl_access_parse_config* parse_ctx) {
	KParamMark* mark = (KParamMark*)parse_ctx->module;
	mark->parse(parse_ctx);
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context* ctx, DWORD notify) {
	KParamMark* mark = (KParamMark*)ctx->module;
	auto filter_ctx = get_input_filter_context(rq, ctx);
	if (!filter_ctx) {
		return KF_STATUS_REQ_TRUE;
	}
	if (mark->get) {
		if (filter_ctx->check_get(mark, rq, ctx)) {
			return KF_STATUS_REQ_FINISHED;
		}
	}
	if (mark->post) {
		filter_ctx->get_filter(rq, ctx)->register_param(mark);
	}
	return KF_STATUS_REQ_TRUE;
}
kgl_access kgl_param_model = {
	sizeof(kgl_access),
	KF_NOTIFY_REQUEST_MARK,
	"param",
	create_access_ctx,
	free_access_ctx,
	build,
	parse,
	NULL,
	process
};