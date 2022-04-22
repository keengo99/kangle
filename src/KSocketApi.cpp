#include "KSocketApi.h"
#include "kconnection.h"
#include "kaddr.h"
#include "kfiber.h"
#include "kselector_manager.h"

static KSOCKET_CLIENT tcp_create(KOPAQUE data)
{
	kconnection *c = kconnection_new(NULL);
	selectable_bind_opaque(&c->st, data, kgl_opaque_other);
	return (KSOCKET_CLIENT)c;
}
static SOCKET get_system_socket(KSOCKET_CLIENT s)
{
	kconnection* c = (kconnection*)s;
	return c->st.fd;
}
KSELECTOR tcp_get_selector(KSOCKET_CLIENT s)
{
	kconnection *c = (kconnection *)s;
	return (KSELECTOR)c->st.selector;
}
void tcp_set_opaque(KSOCKET_CLIENT s, KOPAQUE data)
{
	kconnection *c = (kconnection *)s;
	selectable_bind_opaque(&c->st, data, kgl_opaque_other);
}

kgl_socket_client_function tcp_socket_provider = {
	kfiber_net_getaddr,
	kgl_addr_release,
	get_system_socket,
	(KSOCKET_CLIENT (*)(const struct sockaddr*, socklen_t))kconnection_new3,
	(int (*)(KSOCKET_CLIENT, WSABUF* , int ))kfiber_net_writev,
	(int (*)(KSOCKET_CLIENT, WSABUF* , int))kfiber_net_readv,
	tcp_get_selector,
	tcp_set_opaque,
	(void(*)(KSOCKET_CLIENT))kconnection_destroy,
	(void(*)(KSOCKET_CLIENT))selectable_shutdown,
};

