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
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
class KFetchBigObject;
class KBigObjectContext
{
public:
	KBigObjectContext(KHttpObject* gobj);
	~KBigObjectContext();
#if 0
	/////////////////新接口////////////////////
	kev_result try_handle_request(KHttpRequest *rq);
	kev_result upstream_recv_headed(KHttpRequest *rq, KHttpObject *obj);
	kev_result write_sbo_header_result(KHttpRequest *rq,int got);
	kev_result aio_write_result(KHttpRequest *rq, int got);
	kev_result read_sbo_result(KHttpRequest *rq, char *buf,int got);
	kev_result write_request(KHttpRequest *rq);
	kev_result write_request_end(KHttpRequest *rq);
	/////////////////老接口////////////////////
	int getWriteBuffer(LPWSABUF wbuf,int bufferCount)
	{
		//assert(buffer);
		wbuf[0].iov_base = aio_hot;
		wbuf[0].iov_len = aio_buffer_left;
		return 1;
	}
	kev_result open(KHttpRequest *rq,bool create_flag);
	kev_result send(KHttpRequest *rq);
	kev_result handleWriteResult(KHttpRequest *rq,int got);
	kev_result readBody(KHttpRequest *rq);
	kev_result handleError(KHttpRequest *rq,int code,const char *msg);
	kev_result startNetRequest(KHttpRequest *rq);

	kev_result endRequest(KHttpRequest *rq);
	void saveLastVerified()
	{
		gobj->data->sbo->saveLastVerified(gobj);
	}

private:

	
	kev_result sbo_complete(KHttpRequest *rq);
	kev_result open_cache(KHttpRequest *rq);
	
	kev_result start_send_header(KHttpRequest *rq);
	void prepare_read_buffer()
	{
		if (aio_buffer == NULL || aio_buffer_size<(int)conf.io_buffer) {
			if (aio_buffer) {
				aio_free_buffer(aio_buffer);
			}
			aio_buffer_size = conf.io_buffer;
			aio_buffer = (char *)aio_alloc_buffer(aio_buffer_size);			
		}
		aio_hot = aio_buffer;
		aio_buffer_left = (int)MIN((INT64)aio_buffer_size, left_read);
	}

	char* aio_buffer;
	int aio_buffer_size;
	int aio_buffer_left;
	char* aio_hot;
#endif
	void StartNetRequest(KHttpRequest* rq,kfiber *fiber);
	KGL_RESULT upstream_recv_headed(KHttpRequest* rq, KHttpObject* obj, bool &bigobj_dead);
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
//}}
#endif
