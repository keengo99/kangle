#ifndef KMANTICC_H
#define KMANTICC_H
#include "kforwin32.h"
#include "ksapi.h"
#include "KSessionCtx.h"
#include <string>
#include <sstream>
#define CC_BUFFER_STRING    0
#define CC_BUFFER_URL       1
#define CC_BUFFER_MURL      2
#define CC_BUFFER_REVERT    3
#define CC_BUFFER_SB        4
struct anti_cc_model
{
	const char *name;
	const char *content;
};
static anti_cc_model anti_cc_models[] = {
	{"html redirect","<html><head><meta http-equiv=\"refresh\" content=\"0;url={{url}}\"></head><body><a href=\"{{url}}\">continue</a></body></html>"},
	{"js plain","<html><body><sc'+'ript language=\"javascript\">window.location=\"{{url}}\";</'+'script><a href=\"{{url}}\">continue</a></body></html>"},
	{"js concat","<html><body><sc'+'ript language=\"javascript\">window.location=\"{{murl}}\";</'+'script></body></html>"},
	{"js revert","<html><body><sc'+'ript language=\"javascript\">{{revert:url}};window.location=url;</'+'script></body></html>"},
	{"html manual","<html><body><a href=\"{{url}}\">continue</a></body></html>"},
	{"deny","<html><body>try again later</body></html>"}
};
void * WINAPI  anticc_create_ctx();
void WINAPI anticc_free_ctx(void *ctx);
KGL_RESULT WINAPI  anticc_build(kgl_access_build *build_ctx,enum KF_ACCESS_BUILD_TYPE build_type);
KGL_RESULT WINAPI  anticc_parse(kgl_access_parse *parse_ctx);
DWORD WINAPI anticc_mark_process(kgl_filter_context * ctx,DWORD  type, void * data);
class anticc_context
{
public:
	anticc_context()
	{
		memset(this,0,sizeof(*this));
	}
	~anticc_context()
	{
		if (sc) {
			sc->release();
		}
	}
	bool session_parsed;
	KSessionCtx *sc;
};
#endif

