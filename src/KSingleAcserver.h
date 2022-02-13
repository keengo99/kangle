/*
 * KSingleAcServer.h
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 */

#ifndef KSINGLEACSERVER_H_
#define KSINGLEACSERVER_H_

#include "KAcserver.h"
#include "KSockPoolHelper.h"

class KSingleAcserver: public KPoolableRedirect {
public:
	KSingleAcserver();
	KSingleAcserver(KSockPoolHelper *nodes);
	virtual ~KSingleAcserver();
	unsigned getPoolSize() {
		return sockHelper->getSize();
	}
	bool isChanged(KPoolableRedirect *rd)
	{
		if (KPoolableRedirect::isChanged(rd)) {
			return true;
		}
		KSingleAcserver *sa = static_cast<KSingleAcserver *>(rd);
		return sockHelper->isChanged(sa->sockHelper);
	}
	KUpstream* GetUpstream(KHttpRequest* rq);
	bool setHostPort(std::string host, const char *port);
	const char *getType() {
		return "server";
	}
public:
	void buildXML(std::stringstream &s);
	friend class KAcserverManager;
	KSockPoolHelper *sockHelper;
};
#endif /* KSingleAcserver_H_ */
