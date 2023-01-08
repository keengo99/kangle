#ifndef DSO_FILTER_H_99
#define DSO_FILTER_H_99
#include "ksapi.h"
struct filter_context
{

};
struct sendfile_log_context
{
	volatile int64_t total_length;
	volatile int32_t total_count;
};
void register_filter(KREQUEST r, kgl_access_context *ctx, filter_context *model_ctx);
void register_sendfile_log(KREQUEST r, kgl_access_context* ctx, sendfile_log_context* model_ctx);
extern sendfile_log_context sendfile_context;
#endif
