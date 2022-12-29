#include "KDsoFilter.h"
#include "KAccessDsoSupport.h"
static KGL_RESULT next_flush(KREQUEST rq, KCONN cn)
{
	KDsoFilter *dso_filter = (KDsoFilter *)cn;
	return dso_filter->st->flush((KHttpRequest *)rq);
}
static KGL_RESULT next_write_all(KREQUEST rq, KCONN cn, const char *buf, int size)
{
	KDsoFilter *dso_filter = (KDsoFilter *)cn;
	return dso_filter->st->write_all((KHttpRequest *)rq, buf, size);
}
static KGL_RESULT next_write_end(KREQUEST rq, KCONN cn, KGL_RESULT result)
{
	KDsoFilter *dso_filter = (KDsoFilter *)cn;
	return dso_filter->st->write_end((KHttpRequest *)rq,result);
}
static kgl_filter_conext_function support_function = {
	(kgl_get_variable_f)get_request_variable,
	next_write_all,
	next_flush,
	next_write_end
};
KDsoFilter::KDsoFilter(kgl_filter *filter, void *dso_ctx)
{
	memset(&ctx, 0, sizeof(ctx));
	this->filter = filter;
	ctx.cn = this;
	ctx.module = dso_ctx;
	ctx.f = &support_function;
}
KDsoFilter::~KDsoFilter()
{
	filter->release(ctx.module);
}
KGL_RESULT KDsoFilter::flush(void*rq)
{
	return filter->flush(rq,&ctx);
}
KGL_RESULT KDsoFilter::write_all(void*rq, const char *buf, int len)
{
	return filter->write_all(rq, &ctx, buf, len);
}
KGL_RESULT KDsoFilter::write_end(void *rq, KGL_RESULT result)
{
	return filter->write_end(rq, &ctx, result);
}
