/*
 * KIpVirtualHost.h
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */

#ifndef KIPVIRTUALHOST_H_
#define KIPVIRTUALHOST_H_
#include <map>
#include "KVirtualHostContainer.h"
#include "global.h"
#include "KMutex.h"
#include "ksocket.h"
#ifdef ENABLE_BASED_IP_VH
class KVirtualHost;
class KIpVirtualHost {
public:
	KIpVirtualHost();
	virtual ~KIpVirtualHost();
	bool parseVirtualHost(KHttpRequest *rq,const char *site);
	bool addVirtualHost(KVirtualHost *vh);
	bool removeVirtualHost(KVirtualHost *vh);
	int isEmpty() {
		return true;
	}
private:
	bool bindVirtualHost(const char *ip, KVirtualHost *vh);
	bool unbindVirtualHost(const char *ip, KVirtualHost *vh);
	std::map<sockaddr_i, KVirtualHostContainer *> vhs;
	KMutex lock;
};
#endif
#endif /* KIPVIRTUALHOST_H_ */
