#ifndef KHTTP2SINK_H_99
#define KHTTP2SINK_H_99
#include "KSink.h"
#include "KHttp2.h"
#ifdef ENABLE_HTTP2
class KHttp2Sink : public KSink {
public:
	KHttp2Sink(KHttp2 *http2,KHttp2Context *ctx)
	{
		this->http2 = http2;
		this->ctx = ctx;
	}
	~KHttp2Sink()
	{
		kassert(ctx == NULL);
	}
	bool SetTransferChunked()
	{
		return false;
	}
	bool ResponseStatus(KHttpRequest *rq, uint16_t status_code)
	{
		return http2->add_status(ctx, status_code);
	}
	bool ResponseHeader(const char *name, int name_len, const char *val, int val_len)
	{
		return http2->add_header(ctx, name, name_len,val, val_len);
	}
	bool ResponseConnection(const char *val, int val_len)
	{
		return false;
	}
	//返回头长度,-1表示出错
	int StartResponseBody(KHttpRequest *rq, int64_t body_size)
	{
		ctx->SetContentLength(body_size);
		return http2->send_header(ctx);
	}
	bool IsLocked()
	{
		if (ctx->read_wait) {
			return true;
		}
		if (ctx->write_wait && !IS_WRITE_WAIT_FOR_HUP(ctx->write_wait)) {
			return true;
		}
		return false;
	}
	bool ReadHup(void *arg, result_callback result)
	{
		http2->read_hup(ctx, result, arg);
		return true;
	}
	void RemoveReadHup()
	{
		http2->remove_read_hup(ctx);
	}
	int Read(char *buf, int len)
	{
		WSABUF buffer;
		buffer.iov_base = buf;
		buffer.iov_len = len;
		return http2->read(ctx, &buffer, 1);
	}
	bool HasHeaderDataToSend()
	{
		return false;
	}	
	int Write(LPWSABUF buf, int bc)
	{
		return http2->write(ctx, buf, bc);
	}
	sockaddr_i *GetAddr()
	{
		return &http2->c->addr;
	}
	bool GetRemoteIp(char *ips, int ips_len)
	{
		sockaddr_i *addr = GetAddr();
		return ksocket_sockaddr_ip(addr, ips, ips_len);
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
	bool GetSelfAddr(sockaddr_i *addr)
	{
		return 0 == kconnection_self_addr(http2->c, addr);
	}

	int EndRequest(KHttpRequest *rq)
	{
		KBIT_SET(rq->flags, RQ_CONNECTION_CLOSE);
		if (unlikely(rq->ctx->body_not_complete)) {
			http2->shutdown(ctx);
		} else {
			http2->write_end(ctx);
		}
		http2->release(ctx);
#ifndef NDEBUG
		ctx = NULL;
#endif
		delete rq;
		return 0;
	}
	void AddSync()
	{
	}
	void RemoveSync()
	{
	}
	void Shutdown()
	{
		return http2->shutdown(ctx);
	}
	kconnection *GetConnection()
	{
		return http2->c;
	}
	void SetTimeOut(int tmo_count)
	{
		ctx->tmo = tmo_count;
		ctx->tmo_left = tmo_count;
	}
	int GetTimeOut()
	{
		return ctx->tmo;
	}
	void Flush()
	{
	}
#ifdef ENABLE_PROXY_PROTOCOL
	const char *GetProxyIp()
	{
		return NULL;
	};
#endif
protected:
	void SetReadDelay(bool delay_flag)
	{
	}
	void SetWriteDelay(bool delay_flag)
	{

	}
	friend class KHttp2;
private:
	KHttp2Context *ctx;
	KHttp2 *http2;
};
#endif
#endif
