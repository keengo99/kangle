#include "KDsoRedirect.h"
#include "KDsoAsyncFetchObject.h"

KFetchObject *KDsoRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file)
{
	if (KBIT_TEST(us->flags, KF_UPSTREAM_SYNC)) {
		//not support
		return NULL;
	}
	return new KDsoAsyncFetchObject(NULL, ((kgl_async_upstream *)us)->check);
}
KFetchObject *KDsoRedirect::makeFetchObject(KHttpRequest *rq, void *model_ctx)
{
	if (KBIT_TEST(us->flags, KF_UPSTREAM_SYNC)) {
		//not support
		return NULL;
	}
	return new KDsoAsyncFetchObject(model_ctx,((kgl_async_upstream *)us)->check);
}