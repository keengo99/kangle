#include "KDsoRedirect.h"
#include "KDsoAsyncFetchObject.h"

KRedirectSource*KDsoRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file)
{
	return new KDsoAsyncFetchObject(NULL, KBIT_TEST(((kgl_upstream *)us)->flags, KGL_UPSTREAM_BEFORE_CACHE));
}
KRedirectSource *KDsoRedirect::makeFetchObject(KHttpRequest *rq, void *model_ctx)
{
	return new KDsoAsyncFetchObject(model_ctx, KBIT_TEST(((kgl_upstream*)us)->flags, KGL_UPSTREAM_BEFORE_CACHE));
}