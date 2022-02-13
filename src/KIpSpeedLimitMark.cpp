#include "KIpSpeedLimitMark.h"
void ip_speed_limit_clean(void *data)
{
	KIpSpeedLimitContext *ctx = (KIpSpeedLimitContext *)data;
	ctx->mark->requestClean(ctx->ip);
	free(ctx->ip);
	ctx->mark->release();
	delete ctx;
}