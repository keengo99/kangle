#include "KDsoRedirect.h"
#include "KDsoAsyncFetchObject.h"

KFetchObject *KDsoRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file)
{
	return new KDsoAsyncFetchObject(NULL, ((kgl_async_upstream *)us)->check);
}
KFetchObject *KDsoRedirect::makeFetchObject(KHttpRequest *rq, void *model_ctx)
{
	return new KDsoAsyncFetchObject(model_ctx,((kgl_async_upstream *)us)->check);
}