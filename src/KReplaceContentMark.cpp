#include "KReplaceContentMark.h"
int replaceContentMarkMatchCallBack(void *param, const char *buf, int len, int *ovector, int overctor_size)
{
	KReplaceContentMarkParam *rp = (KReplaceContentMarkParam *)param;
	return rp->mark->match(rp->rq,buf, len, ovector, overctor_size);
}
bool replaceContentMarkCallBack(void *param,KRegSubString *sub_string,int *ovector,KStringBuf *st)
{
	KReplaceContentMarkParam *rp = (KReplaceContentMarkParam *)param;
	return rp->mark->callBack(rp->rq,sub_string,ovector,st);
}
void replaceContentMarkEndCallBack(void *param)
{
	KReplaceContentMarkParam *rp = (KReplaceContentMarkParam *)param;
	rp->mark->release();
	delete rp;
}
