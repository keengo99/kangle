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
	KSingleAcserver(const KString &name);
	KSingleAcserver(KSockPoolHelper *nodes);
	virtual ~KSingleAcserver();
	unsigned getPoolSize() {
		return sockHelper->getSize();
	}
	bool isChanged(KPoolableRedirect *rd) override
	{
		if (KPoolableRedirect::isChanged(rd)) {
			return true;
		}
		KSingleAcserver *sa = static_cast<KSingleAcserver *>(rd);
		return sockHelper->isChanged(sa->sockHelper);
	}
	KUpstream* GetUpstream(KHttpRequest* rq) override;
	bool setHostPort(std::string host, const char *port);
	const char* getType() override {
		return "server";
	}
	bool parse_config(const khttpd::KXmlNode* node) override;
	void set_proto(Proto_t proto) override;
	void shutdown() override {
		sockHelper->shutdown();
	}
public:
	KSockPoolHelper *sockHelper;
};
using KSafeSingleAcserver = KSharedObj<KSingleAcserver>;
#endif /* KSingleAcserver_H_ */
