#ifndef KMSESSIONACCESS_H
#define KMSESSIONACCESS_H
#include <sstream>
#include "ksapi.h"
class KSessionAccess
{
public:
	KSessionAccess()
	{
		memset(this,0,sizeof(*this));
	}
	~KSessionAccess()
	{
		if (name) {
			free(name);
		}
		if (value) {
			free(value);
		}
	}
	static void build_html(kgl_access_build *build_ctx,KSessionAccess *m)
	{
		std::stringstream s;
		s << "name:<input name='name' value='";
		if (m && m->name) {
			s << m->name;
		}
		s << "'/>";
		s << "value:<input name='value' value='";
		if (m && m->value) {
			s << m->value;
		}
		s << "'/>";
		build_ctx->write_string(build_ctx->server_ctx,s.str().c_str(),s.str().size(),0);
	}
	void build_short(kgl_access_build *build_ctx)
	{
		std::stringstream s;
		if (name) {
			s << name;
		}
		s << ":";
		if (value) {
			s << value;
		}
		build_ctx->write_string(build_ctx->server_ctx,s.str().c_str(),s.str().size(),0);
	}
	void build_xml(kgl_access_build *build_ctx)
	{
		std::stringstream s;
		s << "name='";
		if (name) {
			s << name;
		}
		s << "' value='";
		if (value) {
			s << value;
		}
		s << "'";
		build_ctx->write_string(build_ctx->server_ctx,s.str().c_str(),s.str().size(),0);
	}
	void parse(kgl_access_parse *parse_ctx)
	{
		const char *v = parse_ctx->get_value(parse_ctx->server_ctx,"name");
		if (this->name) {
			free(this->name);
			this->name = NULL;
		}
		if (v) {
			this->name = strdup(v);
		}
		v = parse_ctx->get_value(parse_ctx->server_ctx,"value");
		if (this->value) {
			free(this->value);
			this->value = NULL;
		}
		if (v) {
			this->value = strdup(v);
		}

	}
	char *name;
	char *value;
};
void * WINAPI  session_create_ctx();
void WINAPI session_free_ctx(void *ctx);
KGL_RESULT WINAPI  session_build(kgl_access_build *build_ctx,enum KF_ACCESS_BUILD_TYPE build_type);
KGL_RESULT WINAPI  session_parse(kgl_access_parse *parse_ctx);
DWORD WINAPI session_acl_process(kgl_filter_context * ctx,DWORD  type, void * data);
DWORD WINAPI session_mark_process(kgl_filter_context * ctx,DWORD  type, void * data);
#endif
