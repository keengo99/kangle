#include "KSocketApi.h"
#include "kconnection.h"
#include "kaddr.h"
#include "kselector_manager.h"

static KTCP_SOCKET tcp_create(KOPAQUE data)
{
	kconnection *c = kconnection_new(NULL);
	selectable_bind_opaque(&c->st, data, kgl_opaque_other);
	return (KTCP_SOCKET)c;
}
static kev_result tcp_real_connect(kconnection *c)
{
	if (!kconnection_half_connect(c, NULL, 0)) {
		return c->st.e[OP_WRITE].result(c->st.data, c->st.e[OP_WRITE].arg, -1);
	}
	selectable_bind(&c->st, kgl_get_tls_selector());
	return kconnection_connect(c, c->st.e[OP_WRITE].result, c->st.e[OP_WRITE].arg);
}
kev_result tcp_addr_call_back(void *arg, struct addrinfo  *addr)
{
	kconnection *c = (kconnection *)arg;
	if (addr == NULL) {
		return c->st.e[OP_WRITE].result(c->st.data, c->st.e[OP_WRITE].arg, ST_ERR_RESOLV);
	}
	ksocket_addrinfo_sockaddr(addr, c->addr.v4.sin_port, &c->addr);
	return tcp_real_connect(c);
}
static kev_result resolv_dns(void *arg, const char *host, kgl_addr_call_back result)
{
	struct addrinfo *res = NULL;
#ifndef KSOCKET_IPV6
	ai_family = PF_INET;
#endif
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
	f.ai_family = AF_UNSPEC;
	f.ai_flags = AI_NUMERICHOST;
	getaddrinfo(host, NULL, &f, &res);
	if (res == NULL) {
		kselector *selector = kgl_get_tls_selector();
		if (selector == NULL) {
			selector = get_perfect_selector();
		}
		return kgl_find_addr(host, kgl_addr_ip, result, arg, selector);
	}
	kev_result r = result(arg, res);
	freeaddrinfo(res);
	return r;
}
static kev_result tcp_connect_address(KTCP_SOCKET s, struct addrinfo *addr, uint16_t port)
{
	kconnection *c = (kconnection *)s;
	ksocket_addrinfo_sockaddr(addr, c->addr.v4.sin_port, &c->addr);
	return tcp_real_connect(c);
}
static kev_result tcp_connect(KTCP_SOCKET s, const char *host, uint16_t port, result_callback result, void *arg)
{
	kconnection *c = (kconnection *)s;
	bool is_numerichost = ksocket_getaddr(host, port, AF_UNSPEC, AI_NUMERICHOST, &c->addr);
	c->st.e[OP_WRITE].result = result;
	c->st.e[OP_WRITE].arg = arg;
	if (is_numerichost) {
		return tcp_real_connect(c);
	}
	//save port
	c->addr.v4.sin_port = port;
	return kgl_find_addr(host, kgl_addr_ip, tcp_addr_call_back, c, kgl_get_tls_selector());	
}
kev_result tcp_write(KTCP_SOCKET s, result_callback result, buffer_callback buffer, void *arg)
{
	kconnection *c = (kconnection *)s;
	return selectable_write(&c->st, result, buffer, arg);
}
kev_result tcp_read(KTCP_SOCKET s, result_callback result, buffer_callback buffer, void *arg)
{
	kconnection *c = (kconnection *)s;
	return selectable_read(&c->st, result, buffer, arg);
}
bool tcp_is_locked(KTCP_SOCKET s)
{
	kconnection *c = (kconnection *)s;
	return selectable_is_locked(&c->st);
}
KSELECTOR tcp_get_selector(KTCP_SOCKET s)
{
	kconnection *c = (kconnection *)s;
	return (KSELECTOR)c->st.selector;
}
void tcp_set_opaque(KTCP_SOCKET s, KOPAQUE data)
{
	kconnection *c = (kconnection *)s;
	selectable_bind_opaque(&c->st, data, kgl_opaque_other);
}
void tcp_close(KTCP_SOCKET s)
{
	kconnection *c = (kconnection *)s;
	kconnection_destroy(c);
}
void tcp_shutdown(KTCP_SOCKET s)
{
	kconnection *c = (kconnection *)s;
	selectable_shutdown(&c->st);
}
static KGL_RESULT get_remote_address(KTCP_SOCKET s, char *buf, int32_t *buf_size, uint16_t *port)
{
	kconnection *c = (kconnection *)s;
	return KGL_ENOT_SUPPORT;
}
static KGL_RESULT get_locale_address(KTCP_SOCKET s, char *buf, int32_t *buf_size, uint16_t *port)
{
	kconnection *c = (kconnection *)s;
	return KGL_ENOT_SUPPORT;
}
kgl_tcp_socket_function tcp_socket_provider = {
	tcp_create,
	resolv_dns,
	tcp_connect_address,
	tcp_connect,
	tcp_write,
	tcp_read,
	tcp_is_locked,
	tcp_get_selector,
	tcp_set_opaque,
	tcp_close,
	tcp_shutdown,
	get_remote_address,
	get_locale_address
};
