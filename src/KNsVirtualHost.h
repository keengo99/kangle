/*
 * KNsVirtualHost.h
 *
 *  Created on: 2010-5-12
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#ifndef KNSVIRTUALHOST_H_
#define KNSVIRTUALHOST_H_
#include <map>
#include <vector>
#include "global.h"
#include "KVirtualHost.h"
#include "kforwin32.h"
#include "KMutex.h"
#include "KJump.h"
#include "KBaseVirtualHost.h"

class KHttpRequest;
#if 0
class KNsVirtualHost {
public:
	KNsVirtualHost();
	virtual ~KNsVirtualHost();
	bool parseVirtualHost(KHttpRequest *rq);
	KSubVirtualHost *refsVirtualHost(const char *site);
	bool unbindVirtualHost(KSubVirtualHost *vh);
	bool isEmpty() {
		if (hosts.size() == 0 && wideHosts.size() == 0 && defaultVh == NULL) {
			return true;
		}
		return false;
	}
	//no lock
	bool bindVirtualHost(KSubVirtualHost *vh);
	void clear();
	bool addVirtualHost(KVirtualHost *vh);
	bool removeVirtualHost(KVirtualHost *vh);
	//under what port?
	u_short port;
private:
	std::map<char *, KSubVirtualHost *, lessp_icase> hosts;
	std::map<char *, KSubVirtualHost *, lessp_icase> wideHosts;
	KSubVirtualHost *defaultVh;
};
#endif
#endif /* KNSVIRTUALHOST_H_ */
