#include "KConnectionLimit.h"
#include "KHttpRequest.h"
void connection_limit_clean_call_back(void *data)
{
	KConnectionLimit *cl = (KConnectionLimit *)data;
	cl->releaseConnection();
}
bool KConnectionLimit::addConnection(KHttpRequest *rq, int max_connect)
{
	if ((int)katom_inc((void *)&cur_connect) > max_connect) {
		katom_dec((void *)&cur_connect);
		return false;
	}
	addRef();
	rq->registerRequestCleanHook(connection_limit_clean_call_back,this);
	return true;
}