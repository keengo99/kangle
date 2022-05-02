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
	KGL_RESULT buildHead(KHttpRequest *rq)
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
		rq->sink->data.AddHeader(kgl_expand_string("Referer"), s.getString(),s.getSize());
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_UPGRADE);
#ifdef ENABLE_PROXY_PROTOCOL
		if (proxy_protocol) {
			kbuf* buf = build_proxy_header(rq->getClientIp());
			if (buf == NULL) {
				rq->sink->Shutdown();
				return KGL_EUNKNOW;
			}
			buffer->Append(buf);
		}
#endif
		return KGL_OK;
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
	KGL_RESULT buildHead(KHttpRequest *rq)
	{
		rq->ctx->connection_connect_proxy = true;
		KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_UPGRADE);
		buffer = new KSocketBuffer();
		return KGL_OK;
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

