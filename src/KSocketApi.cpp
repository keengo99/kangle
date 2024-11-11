#include "KSocketApi.h"
#include "kconnection.h"
#include "kaddr.h"
#include "kfiber.h"
#include "kselector_manager.h"

static SOCKET get_system_socket(KSOCKET_CLIENT s)
{
	kconnection* c = (kconnection*)s;
	return c->st.fd;
}
KSELECTOR tcp_get_selector(KSOCKET_CLIENT s)
{
	kconnection *c = (kconnection *)s;
	return (KSELECTOR)c->st.base.selector;
}
void tcp_set_opaque(KSOCKET_CLIENT s, KOPAQUE data)
{
	kconnection *c = (kconnection *)s;
	selectable_bind_opaque(&c->st, data);
}
static KOPAQUE tcp_get_opaque(KSOCKET_CLIENT s) {
	kconnection* c = (kconnection*)s;
	return selectable_get_opaque(&c->st);
}
static int tcp_connect(KSOCKET_CLIENT s, const char* local_ip, uint16_t local_port) {
	kconnection* c = (kconnection*)s;
	sockaddr_i* bind_address = nullptr;
	sockaddr_i local_address;
	if (local_ip && *local_ip) {
		bind_address = &local_address;
		if (!ksocket_getaddr(local_ip, local_port, AF_UNSPEC, AI_NUMERICHOST, bind_address)) {
			return -1;
		}
	}
	return kfiber_net_connect(c, bind_address, 0);
}
kgl_socket_client_function tcp_socket_provider = {
	kfiber_net_getaddr,
	kgl_addr_release,
	get_system_socket,
	(KSOCKET_CLIENT(*)(const struct sockaddr*, socklen_t))kconnection_new3,
	(KSOCKET_CLIENT(*)(struct addrinfo* ai, uint16_t port))kconnection_new2,
	tcp_connect,
	(int (*)(KSOCKET_CLIENT, WSABUF* , int))kfiber_net_readv,
	(int (*)(KSOCKET_CLIENT, WSABUF* , int))kfiber_net_writev,
	(bool (*)(KSOCKET_CLIENT, char* , int*))kfiber_net_read_full,
	(size_t (*)(KSOCKET_CLIENT, WSABUF* , int *))kfiber_net_writev_full,
	tcp_get_selector,
	tcp_set_opaque,
	tcp_get_opaque,
	(void(*)(KSOCKET_CLIENT))selectable_shutdown,
	(void(*)(KSOCKET_CLIENT))kconnection_destroy,
};

