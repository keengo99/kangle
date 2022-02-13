/*
 * KIpVirtualHost.cpp
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */
#include <string.h>
#include "KIpVirtualHost.h"
#include "kmalloc.h"
#include "KHttpRequest.h"
#include "KVirtualHost.h"
using namespace std;
#ifdef ENABLE_BASED_IP_VH
KIpVirtualHost::KIpVirtualHost() {
}

KIpVirtualHost::~KIpVirtualHost() {
	std::map<sockaddr_i, KVirtualHostContainer *>::iterator it;
	for (it = vhs.begin(); it != vhs.end(); it++) {
		delete (*it).second;
	}
}
bool KIpVirtualHost::parseVirtualHost(KHttpRequest *rq, const char *site) {
	sockaddr_i addr;
	bool result = false;
	std::map<sockaddr_i, KVirtualHostContainer *>::iterator it;
	rq->sink->get_self_addr(&addr);
	lock.Lock();
	it = vhs.find(addr);
	if (it != vhs.end()) {
		result = (*it).second->parseVirtualHost(rq,site);
	}
	lock.Unlock();
	return result;
}

bool KIpVirtualHost::bindVirtualHost(const char *ip, KVirtualHost *vh) {
	sockaddr_i addr;
	if (!KSocket::getaddr(ip,0,&addr)) {
		return false;
	}
	std::map<sockaddr_i, KVirtualHostContainer *>::iterator it;
	it = vhs.find(addr);
	if (it != vhs.end()) {
		(*it).second->addVirtualHost(vh);
		return true;
	}
	KVirtualHostContainer *nvh = new KVirtualHostContainer;
	nvh->addVirtualHost(vh);
	if (nvh->isEmpty()) {
		delete nvh;
		return false;
	}
	vhs.insert(pair<sockaddr_i, KVirtualHostContainer *> (addr, nvh));
	return true;
}
bool KIpVirtualHost::addVirtualHost(KVirtualHost *vh) {
	std::list<std::string>::iterator it2;
	bool result = false;
	for(it2=vh->binds.begin();it2!=vh->binds.end();it2++){
		const char *bind = (*it2).c_str();
		if (*bind=='#') {
			lock.Lock();
			bindVirtualHost(bind+1,vh);
			lock.Unlock();
			result = true;
		}
	}
	return result;
}
bool KIpVirtualHost::removeVirtualHost(KVirtualHost *vh) {
	std::map<sockaddr_i, KVirtualHostContainer *>::iterator it,it_next;
	for (it=vhs.begin();it!=vhs.end();) {
		(*it).second->removeVirtualHost(vh);
		if ((*it).second->isEmpty()) {
			it_next = it;
			it_next ++;
			vhs.erase(it);
			it = it_next;
		} else {
			it ++;
		}
	}
	return false;
}
#endif
