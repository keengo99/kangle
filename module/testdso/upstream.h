#ifndef DSO_UPSTREAM_H_99
#define DSO_UPSTREAM_H_99
#include "ksapi.h"
enum test_model {
	test_upstream,
	test_forward,
	test_next,
	test_upload_sleep_forward,
	test_websocket,
	test_chunk
};
class test_context {
public:
	test_context(test_model model) {
		this->model = model;
		this->read_post = false;
	}
	test_model model;
	bool read_post;
};
void register_global_async_upstream(kgl_dso_version *ver);
void register_async_upstream(KREQUEST r, kgl_access_context *ctx, test_context *model_ctx,bool before_cache=false);
#endif
