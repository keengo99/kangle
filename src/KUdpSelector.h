#ifndef KUDPSELECTOR_H
#define KUDPSELECTOR_H
#include "global.h"
#include "KSocket.h"
#include <stdlib.h>
#include <string.h>
typedef void (WINAPI *udp_recv_from)(void *arg, const char *data, int len, sockaddr_i *addr, socklen_t addr_len);
struct udp_host_port {
	char *ip;
	unsigned short port;
	udp_host_port *next;
};
bool init_udp_event();
bool udp_recv(udp_host_port *target, const char *package, int package_len, udp_recv_from hook, void *arg);
#endif
