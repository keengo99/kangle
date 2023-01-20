#ifndef DSO_FILTER_H_99
#define DSO_FILTER_H_99
#include "ksapi.h"
struct filter_context
{
	kgl_response_body down;
};
struct sendfile_log_context
{
	kgl_response_body down;
};
void register_filter(KREQUEST r, kgl_access_context *ctx, filter_context *model_ctx);
void register_sendfile_log(KREQUEST r, kgl_access_context* ctx, sendfile_log_context* model_ctx);
extern volatile int64_t sendfile_total_length;
extern volatile int32_t sendfile_total_count;
#endif
