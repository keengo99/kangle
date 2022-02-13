#ifndef KSYNCFETCHOBJECT_H
#define KSYNCFETCHOBJECT_H
#include "KFetchObject.h"
/**
* 同步调用扩展，所有要求同步调用的扩展都从该类继承
*/
#if 0
class KSyncFetchObject : public KFetchObject
{
public:
#ifdef ENABLE_REQUEST_QUEUE
	virtual bool needQueue()
	{
		return true;
	}
#endif
};
#endif
#endif
