#ifndef KBIGOBJECTCONTEXT_H
#define KBIGOBJECTCONTEXT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "kfile.h"
#include "KHttpRequest.h"
#include "KSharedBigObject.h"
#include "KStream.h"
#include "kfile.h"
#include "KMutex.h"
#include "KHttpObject.h"
#include "KSendBuffer.h"
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
class KFetchBigObject;
class KBigObjectContext
{
public:
	KBigObjectContext(KHttpObject* gobj);
	~KBigObjectContext();
	void StartNetRequest(KHttpRequest* rq, kfiber* fiber);
	KGL_RESULT upstream_recv_headed(KHttpRequest* rq, KHttpObject* obj, bool& bigobj_dead);
	void build_if_range(KHttpRequest* rq);
	kfiber* net_fiber;
	void Close(KHttpRequest* rq);
	KGL_RESULT ReadBody(KHttpRequest* rq);
	KGL_RESULT SendHeader(KHttpRequest* rq);
	KGL_RESULT OpenCache(KHttpRequest* rq);
	KGL_RESULT Open(KHttpRequest* rq, bool create_flag);
	KGL_RESULT StreamWrite(INT64 offset, const char* buf, int len);
	int EndRequest(KHttpRequest* rq);
	KSharedBigObject* getSharedBigObject()
	{
		return gobj->data->sbo;
	}
	KHttpObject* gobj;
	friend class KBigObjectStream;
	/**
	* 发送到用户数据偏移
	*/
	INT64 range_from;
	INT64 left_read; //发送到用户数据剩余
	/**
	* 最后的网络请求数据偏移，CloseWrite要用到
	*/
	INT64 last_net_from;
};
#endif
#endif
