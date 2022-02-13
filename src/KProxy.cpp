#include "KHttpRequest.h"
#include "KUpstream.h"
#include "kselector_manager.h"
#include "KProxy.h"
#ifdef ENABLE_PROXY_PROTOCOL
const char kgl_proxy_v2sig[] = "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A";
class KRequestProxy;
typedef kev_result(KRequestProxy::*proxy_state_handler_pt) ();
class KRequestProxy {
public:
	KRequestProxy()
	{
		memset(this, 0, sizeof(*this));
	}
	~KRequestProxy()
	{
		if (body) {
			xfree(body);
		}
	}
	kev_result destroy();
	kev_result state(int got);
	kev_result state_header();
	kev_result state_body();
	proxy_state_handler_pt state_handle;
	kgl_proxy_hdr_v2 hdr;
	char *body;
	char *hot;
	int left;
	kconnection *cn;
	result_callback cb;
};
int buffer_proxy_read(KOPAQUE data, void *arg, LPWSABUF buf, int bc)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	buf->iov_len = proxy->left;
	buf->iov_base = proxy->hot;
	return 1;
}
kev_result result_proxy_reader(KOPAQUE data, void *arg, int got)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	return proxy->state(got);
}
kev_result KRequestProxy::destroy()
{
	kev_result ret = cb(cn->st.data, cn, -1);
	delete this;
	return ret;
}
kev_result KRequestProxy::state_body()
{
	cn->proxy = (kgl_proxy_protocol *)kgl_pnalloc(cn->pool, sizeof(kgl_proxy_protocol));
	cn->proxy->src = (sockaddr_i *)kgl_pnalloc(cn->pool,sizeof(sockaddr_i));
	cn->proxy->dst = (sockaddr_i *)kgl_pnalloc(cn->pool, sizeof(sockaddr_i));
	cn->proxy->data = NULL;
	memset(cn->proxy->src, 0, sizeof(sockaddr_i));
	memset(cn->proxy->dst, 0, sizeof(sockaddr_i));
	if (hdr.fam == 1) {
		if (hdr.len < sizeof(kgl_proxy_ipv4_addr)) {
			return destroy();
		}
		kgl_proxy_ipv4_addr *v4_addr = (kgl_proxy_ipv4_addr *)body;
		cn->proxy->src->v4.sin_family = PF_INET;
		kgl_memcpy(&cn->proxy->src->v4.sin_addr, &v4_addr->src_addr, sizeof(cn->proxy->src->v4.sin_addr));
		cn->proxy->src->v4.sin_port = v4_addr->src_port;

		cn->proxy->dst->v4.sin_family = PF_INET;
		kgl_memcpy(&cn->proxy->dst->v4.sin_addr, &v4_addr->dst_addr, sizeof(cn->proxy->dst->v4.sin_addr));
		cn->proxy->dst->v4.sin_port = v4_addr->dst_port;

	} else if (hdr.fam == 2) {
		if (hdr.len < sizeof(kgl_proxy_ipv6_addr)) {
			return destroy();
		}
		kgl_proxy_ipv6_addr *v6_addr = (kgl_proxy_ipv6_addr *)body;
		cn->proxy->src->v4.sin_family= PF_INET6;
		kgl_memcpy(&cn->proxy->src->v6.sin6_addr, &v6_addr->src_addr, sizeof(cn->proxy->src->v6.sin6_addr));
		cn->proxy->src->v6.sin6_port = v6_addr->src_port;

		cn->proxy->dst->v4.sin_family = PF_INET6;
		kgl_memcpy(&cn->proxy->dst->v6.sin6_addr, &v6_addr->dst_addr, sizeof(cn->proxy->dst->v6.sin6_addr));
		cn->proxy->dst->v6.sin6_port = v6_addr->dst_port;
	} else {
		//not support protocol
		return destroy();
	}
	/*
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(cn->proxy_dst, ips, MAXIPLEN - 1);
	printf("dst ip=[%s:%d]\n", ips, ntohs(cn->proxy_dst->v4.sin_port));
	*/
	kev_result ret = cb(cn->st.data, cn, 0);
	delete this;
	return ret;
}
kev_result KRequestProxy::state_header()
{
	if (memcmp(hdr.sig, kgl_proxy_v2sig, sizeof(hdr.sig)) != 0) {
		//sign error
		return destroy();
	}
	hdr.len = ntohs(hdr.len);
	if (hdr.len > 4096 || hdr.len <sizeof(kgl_proxy_ipv4_addr)) {
		//len is wrong.
		return destroy();
	}
	kassert(body == NULL);
	body = (char *)xmalloc(hdr.len);
	left = hdr.len;
	hot = body;
	uint8_t ver = (hdr.ver_cmd >> 4);
	uint8_t cmd = (hdr.ver_cmd & 0xF);
	if (ver != 2) {
		//version error
		return destroy();
	}
	if (cmd != 1) {
		//only support PROXY command
		return destroy();
	}
	state_handle = &KRequestProxy::state_body;
	return selectable_read(&cn->st, result_proxy_reader, buffer_proxy_read, this);
}
kev_result KRequestProxy::state(int got)
{
	if (got <= 0) {
		destroy();
		return kev_destroy;
	}
	hot += got;
	left -= got;
	if (left > 0) {
		return selectable_read(&cn->st, result_proxy_reader, buffer_proxy_read, this);
	}
	return (this->*state_handle)();
}

kev_result handl_proxy_request(kconnection *cn, result_callback cb)
{
	KRequestProxy *proxy = new KRequestProxy;
	proxy->hot = (char *)&proxy->hdr;
	proxy->left = sizeof(kgl_proxy_hdr_v2);
	proxy->state_handle = &KRequestProxy::state_header;
	proxy->cn = cn;
	proxy->cb = cb;
	return selectable_read(&cn->st,result_proxy_reader, buffer_proxy_read,proxy);
}
bool build_proxy_header(KReadWriteBuffer *buffer, KHttpRequest *rq)
{
	const char *ip = rq->getClientIp();
	ip_addr ia;
	kgl_proxy_hdr_v2 *hdr;
	if (!ksocket_get_ipaddr(ip, &ia)) {
		return false;
	}
	int len;
	kbuf *buf = NULL;
	if (ia.sin_family == PF_INET) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv4_addr);
		buf = new_kbuf(len);
		hdr = (kgl_proxy_hdr_v2 *)buf->data;
		memset(hdr, 0, len);
		hdr->fam = 1;
		kgl_proxy_ipv4_addr *addr = (kgl_proxy_ipv4_addr *)(hdr + 1);
		addr->src_addr = ia.addr32[0];
	} else if (ia.sin_family == PF_INET6) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv6_addr);
		buf = new_kbuf(len);
		hdr = (kgl_proxy_hdr_v2 *)buf->data;
		memset(hdr, 0, len);
		hdr->fam = 2;
		kgl_proxy_ipv6_addr *addr = (kgl_proxy_ipv6_addr *)(hdr + 1);
		kgl_memcpy(addr->src_addr, ia.addr8, sizeof(addr->src_addr));
	} else {
		//not support protocol
		return false;
	}
	kgl_memcpy(hdr->sig, kgl_proxy_v2sig, sizeof(hdr->sig));
	hdr->ver_cmd = (2<<4)|1;
	hdr->len = htons((uint16_t)(len-sizeof(kgl_proxy_hdr_v2)));
	buffer->Append(buf);
	return true;
}
#endif
