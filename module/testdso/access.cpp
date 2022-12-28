#include <string.h>
#include "kstring.h"
#include "ksapi.h"
#include "kmalloc.h"
#include "upstream.h"
#include "filter.h"

static int test_notify_type = KF_NOTIFY_REQUEST_MARK;

static void *create_access_ctx()
{
	return NULL;
}
static void free_access_ctx(void *ctx)
{

}
static KGL_RESULT build(kgl_access_build *build_ctx, KF_ACCESS_BUILD_TYPE build_type)
{
	build_ctx->write_string(build_ctx->cn, "test", 4, 0);
	return KGL_OK;
}
static KGL_RESULT parse(kgl_access_parse *parse_ctx, KF_ACCESS_PARSE_TYPE parse_type)
{
	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context *ctx, DWORD notify)
{
	kassert(notify == test_notify_type);
	char buf[512];
	buf[0] = '\0';
	DWORD len = sizeof(buf);
	if (ctx->f->get_variable(rq, KGL_VAR_PATH_INFO, NULL, buf, &len) == KGL_OK) {
		if (strcmp(buf, "/dso/accept_header") == 0) {
			//test response accept header as x-accept
			len = sizeof(buf);
			if (KGL_OK == ctx->f->get_variable(rq, KGL_VAR_HEADER, "Accept", buf, &len)) {
				ctx->f->response_header(rq, ctx->cn, _KS("x-accept"), buf, (hlen_t)len);
			}
			return KF_STATUS_REQ_FINISHED;
		}
		if (strcmp(buf, "/test_access_deny") == 0) {
			return KF_STATUS_REQ_FINISHED;
		}
		if (strcmp(buf, "/test_dso_upstream") == 0) {
			test_context *model_ctx = new test_context(test_upstream);
			register_async_upstream(rq, ctx, model_ctx);
			return KF_STATUS_REQ_TRUE;
		}
		if (strcmp(buf, "/test_dso_filter") == 0) {

			filter_context *model_ctx = new filter_context;
			register_filter(rq, ctx, model_ctx);

			test_context *model_ctx2 = new test_context(test_upstream);
			register_async_upstream(rq, ctx, model_ctx2);
			return KF_STATUS_REQ_TRUE;
		}
		if (strcmp(buf, "/read_hup") == 0) {
			//这里要sleep一下，以便造成ssl读的时候产生pending数据，测试需要
			test_context *model_ctx = new test_context(test_upload_sleep_forward);
			register_async_upstream(rq, ctx, model_ctx);
			return KF_STATUS_REQ_TRUE;
		}
	}
	test_context *model_ctx = new test_context(test_forward);
	register_async_upstream(rq, ctx, model_ctx);
	return KF_STATUS_REQ_TRUE;
}
static kgl_access access_model = {
	sizeof(kgl_access),test_notify_type,
	"test",
	create_access_ctx,
	free_access_ctx,
	build,
	parse,
	process,
	NULL
};
void register_access(kgl_dso_version *ver)
{	
	KGL_REGISTER_ACCESS(ver, &access_model);
}
