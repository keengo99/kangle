#ifndef KTCPFETCHOBJECT_H
#define KTCPFETCHOBJECT_H
#include "KAsyncFetchObject.h"
#include "KProxy.h"
class KTcpFetchObject : public KAsyncFetchObject
{
public:
	KTcpFetchObject(bool proxy_protocol) : KAsyncFetchObject() {
#ifdef ENABLE_PROXY_PROTOCOL
		this->proxy_protocol = proxy_protocol;
#endif
	}
	void buildHead(KHttpRequest *rq)
	{
		buffer = new KSocketBuffer();
		KStringBuf s;
		sockaddr_i *addr = client->GetAddr();
		char ips[MAXIPLEN];
		memset(ips, 0, sizeof(ips));
		ksocket_sockaddr_ip(addr, ips, sizeof(ips) - 1);
		bool ipv6 = (addr->v4.sin_family == PF_INET6);
		if (ipv6) {
			s << "[";
		}
		s << ips;
		if (ipv6) {
			s << "]";
		}
		s << ":" << ksocket_addr_port(addr);
		rq->AddHeader(kgl_expand_string("Referer"), s.getString(),s.getSize());
		rq->ctx->connection_upgrade = true;
#ifdef ENABLE_PROXY_PROTOCOL
		if (proxy_protocol && !build_proxy_header(buffer, rq)) {
			rq->sink->Shutdown();
			return;
		}
#endif
	}
	Parse_Result parseHead(KHttpRequest *rq, char *buf, int len)
	{
		return Parse_Success;
	}
	bool NeedTempFile(bool upload, KHttpRequest *rq)
	{
		return false;
	}
#ifdef ENABLE_PROXY_PROTOCOL
	bool proxy_protocol;
#endif
};

class KConnectProxyFetchObject : public KAsyncFetchObject
{
public:
	void buildHead(KHttpRequest *rq)
	{
		rq->ctx->connection_connect_proxy = true;
		rq->ctx->connection_upgrade = true;
		buffer = new KSocketBuffer();
	}
	Parse_Result parseHead(KHttpRequest *rq, char *buf, int len)
	{
		return Parse_Success;
	}
	bool NeedTempFile(bool upload, KHttpRequest *rq)
	{
		return false;
	}
};
#endif

