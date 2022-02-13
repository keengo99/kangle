/*
 * KNsVirtualHost.cpp
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

#include "do_config.h"
#include "KNsVirtualHost.h"
#include "KHttpRequest.h"
#include "kmalloc.h"
#include "KVirtualHostManage.h"
#include "lang.h"
#include "KLineFile.h"
#if 0
using namespace std;
KNsVirtualHost::KNsVirtualHost() {
	defaultVh = NULL;
	port = 0;
}

KNsVirtualHost::~KNsVirtualHost() {
	clear();
}

KSubVirtualHost *KNsVirtualHost::refsVirtualHost(const char *site) {
	KSubVirtualHost *vh = NULL;
	//lock.Lock();
	map<char *, KSubVirtualHost *, lessp_icase>::iterator it = hosts.find(
			(char *) site);
	if (it != hosts.end()) {
		vh = (*it).second;
	} else if (wideHosts.size() > 0) {
		const char *wideSite = strchr(site, '.');
		if (wideSite != NULL) {
			it = wideHosts.find((char *) wideSite);
			if (it != wideHosts.end()) {
				vh = (*it).second;
			}
		}
	}
	if (vh == NULL) {
		vh = defaultVh;
	}
	if (vh) {
		vh->vh->addRef();
	}
	return vh;
}
bool KNsVirtualHost::parseVirtualHost(KHttpRequest *rq) {
	assert(rq->svh==NULL);
	rq->svh = refsVirtualHost(rq->url->host);
	if (rq->svh) {
#ifdef ENABLE_VH_RS_LIMIT
		if (!rq->svh->vh->addConnection()) {
			rq->svh->vh->destroy();
			rq->svh = NULL;
			SET(rq->filter_flags,RQ_CONNECT_LIMIT);
			return false;
		}
#endif
#if 0
#ifdef ENABLE_VH_RS_LIMIT
		if (rq->svh->vh->gsl) {
			rq->addGSpeedLimit(rq->svh->vh->gsl);
		}
#endif
#endif
		//rq->fetchObj = new KLocalFetchObject();
		return true;
	}
	return false;
}
void KNsVirtualHost::clear() {
	hosts.clear();
	wideHosts.clear();
	defaultVh = NULL;
}
bool KNsVirtualHost::removeVirtualHost(KVirtualHost *vh) {
	list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		if((*it2)->matchPort(port)){
			unbindVirtualHost((*it2));
		}
	}
	return true;
}
bool KNsVirtualHost::addVirtualHost(KVirtualHost *vh) {

	list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end();it2++) {
		if ( (*it2)->matchPort(port) && !bindVirtualHost((*it2)) ) {
			(*it2)->allSuccess = false;
		}
	}
	return true;
}
bool KNsVirtualHost::unbindVirtualHost(KSubVirtualHost *vh) {
	bool result = false;
	char *site = vh->host;
	map<char *, KSubVirtualHost *, lessp_icase>::iterator it;
	if (strcmp(site, "*") == 0 || strcmp(site,"default")==0) {
		if (vh == defaultVh) {
			defaultVh = NULL;
			return true;
		}
		return false;
	}
	if (site[0] == '*') {
		site = site + 1;
		it = wideHosts.find(site);
		if (it != wideHosts.end() && (*it).second == vh) {
			wideHosts.erase(it);
			result = true;
		}
	} else {
		it = hosts.find(site);
		if (it != hosts.end() && (*it).second == vh) {
			hosts.erase(it);
			result = true;
		}
	}
	return result;
}
bool KNsVirtualHost::bindVirtualHost(KSubVirtualHost *svh) {
	bool result = false;
	map<char *, KSubVirtualHost *, lessp_icase>::iterator it;
	char *site = svh->host;
	if (strcmp(site, "*") == 0 || strcmp(site, "default") == 0) {
		if (defaultVh == NULL) {
			defaultVh = svh;
			return true;
		}
		if (defaultVh == svh) {
			return true;
		}
		return false;
	}
	if (site[0] == '*') {
		site = site + 1;
		it = wideHosts.find(site);
		if (it == wideHosts.end()) {
			wideHosts.insert(pair<char *, KSubVirtualHost *> (site, svh));
			result = true;
		} else if ((*it).second == svh) {
			result = true;
		}
	} else {
		it = hosts.find(site);
		if (it == hosts.end()) {
			hosts.insert(pair<char *, KSubVirtualHost *> (site, svh));
			result = true;
		} else if ((*it).second == svh) {
			result = true;
		}
	}
	return result;
}
#endif
