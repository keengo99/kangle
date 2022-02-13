#include "KUdpSelector.h"
#include "klist.h"
#if 0
#include "KMutex.h"
#ifndef _WIN32
#include <sys/epoll.h>
#endif
#define UDP_TIME_OUT 3
namespace kangle {
	static int kdpfd = -1;
	static KMutex udp_lock;
	static volatile bool udp_event_started = false;
	class KUdpEvent {
	public:
		KUdpEvent()
		{
			memset(this, 0, sizeof(KUdpEvent));
			sockfd = -1;
		}
		~KUdpEvent()
		{
			if (sockfd >= 0) {
				::closesocket(sockfd);
			}
			if (package) {
				free(package);
			}
			if (addr) {
				freeaddrinfo(addr);
			}
		}
		bool send()
		{
			try_time++;
			return ::sendto(sockfd, package, package_len, 0, (sockaddr *)addr->ai_addr, addr->ai_addrlen) > 0;
		}
		int sockfd;
		int try_time;
		int retry;
		time_t last_time;
		udp_recv_from cb;
		void *arg;
		char *package;
		int package_len;
		struct addrinfo *addr;
		KUdpEvent *next;
		KUdpEvent *prev;
	};
	static KUdpEvent *udp_list = NULL;
	static bool check_time_out()
	{
		KUdpEvent *e;
		KUdpEvent *time_out_head = NULL;
		time_t now_time = time(NULL);
		udp_lock.Lock();
		assert(udp_event_started);
		bool empty = true;
		for (;;) {
			e = klist_head(udp_list);
			if (e == udp_list) {
				if (empty) {
					udp_event_started = false;
					udp_lock.Unlock();
					return true;
				}
				break;
			}
			empty = false;
			if (now_time - e->last_time < UDP_TIME_OUT) {
				break;
			}
			klist_remove(e);
			e->next = time_out_head;
			time_out_head = e;
		}
		udp_lock.Unlock();
		while (time_out_head) {
			//printf("time_out_head = [%p]\n",time_out_head);
			KUdpEvent *next = time_out_head->next;
			time_out_head->next = NULL;
			if (time_out_head->send()) {
				time_out_head->last_time = now_time;
				udp_lock.Lock();
				klist_append(udp_list, time_out_head);
				udp_lock.Unlock();
			}
			else {
				time_out_head->cb(time_out_head->arg, NULL, -1,NULL,0);
				delete time_out_head;
			}
			time_out_head = next;
		}
		return false;
	}
}
#endif
