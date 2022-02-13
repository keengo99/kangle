/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KUPSTREAM_H
#define KUPSTREAM_H
#include <time.h>
#include "global.h"
#include "kselector.h"
#include "ksocket.h"
#include "kconnection.h"
#include "kmalloc.h"
#include "kstring.h"
#include "KHttpHeader.h"
enum BadStage
{
	/* BadStage_Connect BadStage_TrySend  */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess  */
	BadStage_SendSuccess,
};

class KPoolableSocketContainer;
class KHttpRequest;
class KWriteStream;
class KAsyncFetchObject;
class KFetchObject;

class KUpstream
{
public:
	KUpstream()
	{
		expire_time = 0;
		container = NULL;
	}
	virtual void SetDelay()
	{

	}
	virtual void SetNoDelay(bool forever)
	{
	}
	virtual KOPAQUE GetOpaque()
	{
		return GetConnection()->st.data;
	}
	kselector* GetSelector()
	{
		return GetConnection()->st.selector;
	}
	virtual void BindOpaque(KOPAQUE data) = 0;
	virtual bool BuildHttpHeader(KHttpRequest *rq, KAsyncFetchObject* fo, KWriteStream *s)
	{
		return false;
	}
	virtual bool SetHttp2Parser(void *arg, kgl_header_callback header)
	{
		return false;
	}
	virtual bool ReadHttpHeader(KGL_RESULT *result)
	{
		return false;
	}
	virtual kgl_pool_t *GetPool()
	{
		return NULL;
	}
	virtual kconnection *GetConnection() = 0;
	virtual void WriteEnd()
	{

	}
	virtual int Read(char* buf, int len) = 0;
	virtual int Write(WSABUF * buf, int bc) = 0;
	virtual void Shutdown() = 0;
	virtual void Destroy() = 0;
	virtual bool IsMultiStream()
	{
		return false;
	}
	virtual void SetTimeOut(int tmo)
	{
	}
	virtual void BindSelector(kselector *selector)
	{
	}
	virtual void OnPushContainer()
	{
	}
	virtual KUpstream *NewStream()
	{
		return NULL;
	}
	virtual bool IsNew() {
		return expire_time == 0;
	}
	virtual int GetLifeTime();
	virtual void IsGood();	
	virtual void IsBad(BadStage stage);
	virtual sockaddr_i *GetAddr() = 0;
	bool GetSelfAddr(sockaddr_i *addr)
	{
		kconnection *cn = GetConnection();
		if (cn == NULL) {
			return false;
		}
		return 0 == kconnection_self_addr(cn, addr);
	}
	uint16_t GetSelfPort()
	{
		sockaddr_i addr;
		if (!GetSelfAddr(&addr)) {
			return 0;
		}
		return ksocket_addr_port(&addr);
	}
	virtual kgl_refs_string *GetParam();
	virtual void Gc(int life_time,time_t base_time) = 0;
	friend class KPoolableSocketContainer;
	time_t expire_time;
	KPoolableSocketContainer *container;
protected:
	virtual ~KUpstream();
};
#define KPoolableUpstream KUpstream
#endif /* KPOOLABLESTREAM_H_ */
