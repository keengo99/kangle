#ifndef KCONTEXTECTION_LIMIT_H
#define KCONTEXTECTION_LIMIT_H
#include "KCountable.h"
class KHttpRequest;
/**
* 连接数限制类
*/
class KConnectionLimit : public KAtomCountable
{
public:
	KConnectionLimit()
	{
		cur_connect = 0;
	}
	bool addConnection(KHttpRequest *rq, int max_connect);
	void releaseConnection()
	{
		katom_dec((void *)&cur_connect);
		release();
	}
	int getConnectionCount()
	{
		return (int)katom_get((void *)&cur_connect);
	}
private:
	volatile int32_t cur_connect;
};
#endif
