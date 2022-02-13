#ifndef KTSUPSTREAM_H
#define KTSUPSTREAM_H
#include "KUpstream.h"


class KTsUpstreamStack {
public:
	KTsUpstreamStack()
	{
		memset(this, 0, sizeof(KTsUpstreamStack));
	}
	kgl_header_callback header;
	void *arg;
};
//thread safe upstream
class KTsUpstream : public KUpstream {
public:
	KTsUpstream(KUpstream *us)
	{
		this->us = us;
		header = NULL;
	}
	~KTsUpstream()
	{
		kassert(us == NULL);
	}
	void SetDelay()
	{
		us->SetDelay();
	}
	void SetNoDelay(bool forever)
	{
		us->SetNoDelay(forever);
	}
	kconnection *GetConnection()
	{
		return us->GetConnection();
	}
	bool SetHttp2Parser(void *arg, kgl_header_callback cb);
	bool BuildHttpHeader(KHttpRequest *rq, KAsyncFetchObject *fo, KWriteStream *s)
	{
		return us->BuildHttpHeader(rq, fo, s);
	}
	void SetTimeOut(int tmo)
	{
		return us->SetTimeOut(tmo);
	}
	bool ReadHttpHeader(KGL_RESULT* result);
	KOPAQUE GetOpaque()
	{
		return us->GetOpaque();
	}
	void BindOpaque(KOPAQUE data)
	{
		us->BindOpaque(data);
	}
	int Write(WSABUF* buf, int bc);
	int Read(char* buf, int len);

	bool IsMultiStream()
	{
		return us->IsMultiStream();
	}
	bool IsNew() {
		return us->IsNew();
	}
	int GetLifeTime()
	{
		return us->GetLifeTime();
	}
	void IsGood()
	{
		return us->IsGood();
	}
	void IsBad(BadStage stage)
	{
		return us->IsBad(stage);
	}
	kgl_refs_string *GetParam()
	{
		return us->GetParam();
	}
	void WriteEnd();
	void Shutdown();
	void Destroy();
	bool IsLocked();
	sockaddr_i *GetAddr()
	{
		return us->GetAddr();
	}
	int GetPoolSid()
	{
		return 0;
	}
	kgl_pool_t *GetPool()
	{
		return us->GetPool();
	}
	void Gc(int life_time,time_t last_recv_time);
	KTsUpstreamStack stack;
	KUpstream *us;
	KHttpHeader *header;
	KHttpHeader *last_header;
};
#endif
