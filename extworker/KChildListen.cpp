/*
 * KChildListen.cpp
 *
 *  Created on: 2010-7-10
 *      Author: keengo
 */
#include <vector>
#include "KChildListen.h"
#include "log.h"
#include "api_child.h"

KChildListen::KChildListen() {
	st = NULL;
	ksocket_init(sockfd);
}
KChildListen::~KChildListen() {
	if (ksocket_opened(sockfd)) {
		ksocket_close(sockfd);
	}
}
bool KChildListen::canRead()
{
	sockaddr_i addr;
	SOCKET s = ksocket_accept(this->sockfd, &addr, false);
	//debug("ksocket_accept result sockfd=[%d]\n", (int)s);
	if (!ksocket_opened(s)) {
		return false;
	}
	KSocketStream *ss = new KSocketStream(s);
	if (!kthread_start(api_child_thread, ss)) {
		delete ss;
		return false;
	}
	return true;
}

