#ifndef KSINK_H_99
#define KSINK_H_99
#include "global.h"
#include "KStream.h"
#include "kselector.h"
#include "kserver.h"
#include "kmalloc.h"
#include "KResponseContext.h"
#include "kgl_ssl.h"
#include "kstring.h"

class KHttpRequest;

class KSink {
public:
	virtual ~KSink()
	{

	}
	virtual bool ReadHup(void *arg, result_callback result) = 0;
	virtual void RemoveReadHup() = 0;
	virtual bool HasHeaderDataToSend()
	{
		return false;
	}
	virtual int GetReadPending() {
		return 0;
	}
	virtual bool SetTransferChunked() {
		return ResponseHeader(kgl_expand_string("Transfer-Encoding"), kgl_expand_string("chunked"));
	}
	kgl_pool_t *GetConnectionPool()
	{
		return GetConnection()->pool;
	}
	virtual sockaddr_i *GetAddr()
	{
		kconnection *cn = GetConnection();	
		return &cn->addr;		
	}
	bool GetRemoteIp(char *ips, int ips_len)
	{
		sockaddr_i *addr = GetAddr();
		return ksocket_sockaddr_ip(addr,  ips, ips_len);
	}
	uint16_t GetSelfPort() {
		sockaddr_i addr;
		if (!GetSelfAddr(&addr)) {
			return 0;
		}
		return ksocket_addr_port(&addr);
	}
	bool GetSelfIp(char *ips, int ips_len)
	{
		sockaddr_i addr;
		if (!GetSelfAddr(&addr)) {
			return false;
		}
		return ksocket_sockaddr_ip(&addr, ips, ips_len);
	}
	virtual bool GetSelfAddr(sockaddr_i *addr)
	{
		return 0 == kconnection_self_addr(GetConnection(), addr);
	}
	virtual void AddSync() = 0;
	virtual void RemoveSync() = 0;
	kselector *GetSelector()
	{
		return GetConnection()->st.selector;
	}
	virtual void Shutdown() = 0;
	kserver *GetBindServer()
	{
		return GetConnection()->server;
	}
	virtual kconnection *GetConnection() = 0;
	virtual void SetTimeOut(int tmo_count) = 0;
	virtual void AddTimer(result_callback result, void *arg, int msec)
	{
		kselector *selector = GetSelector();
		kselector_add_timer(selector, result, arg, msec, NULL);
	}
	virtual int GetTimeOut() = 0;
	virtual void SetDelay()
	{

	}
	virtual void SetNoDelay(bool forever)
	{
	}
	void Flush()
	{
		SetNoDelay(false);
		SetDelay();
	}
#ifdef KSOCKET_SSL
	kssl_session *GetSSL() {
		kconnection *cn = GetConnection();
		return cn->st.ssl;
	}
#endif
	virtual void StartHeader(KHttpRequest *rq)
	{
		return;
	}
	friend class KHttpRequest;
protected:
	virtual int EndRequest(KHttpRequest *rq) = 0;
	int Write(const char *buf, int len)
	{
		WSABUF b;
		b.iov_base = (char *)buf;
		b.iov_len = len;
		return Write(&b, 1);
	}
	virtual int Write(LPWSABUF buf, int bc) = 0;
	virtual int Read(char *buf, int len) = 0;
	virtual bool ResponseStatus(KHttpRequest *rq, uint16_t status_code) = 0;
	virtual bool ResponseHeader(const char *name, int name_len, const char *val, int val_len) = 0;
	virtual bool ResponseConnection(const char *val, int val_len) = 0;
	virtual int StartResponseBody(KHttpRequest *rq, int64_t body_size) = 0;
};
#endif
