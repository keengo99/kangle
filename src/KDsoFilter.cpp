#include "KDsoFilter.h"
#include "KAccessDsoSupport.h"
#if 0
static KGL_RESULT next_flush(KREQUEST rq, KCONN cn)
{
	return ((KWriteStream*)cn)->flush((KHttpRequest *)rq);
}
static KGL_RESULT next_write_all(KREQUEST rq, KCONN cn, const char *buf, int size)
{
	return ((KWriteStream*)cn)->write_all((KHttpRequest *)rq, buf, size);
}
static KGL_RESULT next_sendfile(KREQUEST rq, KCONN cn, KASYNC_FILE fp, int64_t* length) {
	return ((KWriteStream*)cn)->sendfile((void*)rq, (kfiber_file*)fp, length);
}
static KGL_RESULT next_write_end(KREQUEST rq, KCONN cn, KGL_RESULT result)
{
	return ((KWriteStream*)cn)->write_end((KHttpRequest *)rq,result);
}
static bool next_support_sendfile(KREQUEST rq, KCONN cn) {
	return ((KWriteStream*)cn)->support_sendfile(rq);
}
static kgl_filter_conext_function support_function = {
	(kgl_get_variable_f)get_request_variable,
	next_write_all,
	next_flush,
	next_support_sendfile,
	next_sendfile,
	next_write_end
};
KDsoFilter::KDsoFilter(kgl_filter *filter, void *dso_ctx)
{
	memset(&ctx, 0, sizeof(ctx));
	this->filter = filter;
	ctx.cn = st;
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
KGL_RESULT KDsoFilter::sendfile(void* rq, kfiber_file* fp, int64_t* len) {
	assert(filter->sendfile);
	return filter->sendfile(rq, &ctx, (KASYNC_FILE)fp, len);
}
bool KDsoFilter::support_sendfile(void* rq) {
	if (!filter->sendfile) {
		return false;
	}
	return KHttpStream::forward_support_sendfile(rq);
}
#endif

