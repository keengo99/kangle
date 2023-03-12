/*
 * KMultiAcserver.h
 *
 *  Created on: 2010-6-4
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

#ifndef KMULTIACSERVER_H_
#define KMULTIACSERVER_H_
#include <vector>
#include "KAcserver.h"
#include "global.h"
#include "KSockPoolHelper.h"
#include "KConfigTree.h"

class KMultiAcserver: public KPoolableRedirect, public kconfig::KConfigListen {
public:
	KMultiAcserver(const KString &name);
	KMultiAcserver(KSockPoolHelper *nodes);
	virtual ~KMultiAcserver();
public:
	void shutdown() override;
	KUpstream* GetUpstream(KHttpRequest* rq) override;
	unsigned getPoolSize();
	bool editNode(KXmlAttribute&attr);
	bool parse_config(const khttpd::KXmlNode* xml) override;
	void buildAttribute(KWStream&s);
	static void baseHtml(KMultiAcserver *mserver, KWStream&s);
	void getHtml(KWStream &s);
	void getNodeInfo(KWStream&s);
	friend class KAcserverManager;
	static void form(KMultiAcserver *mserver,KWStream &s);
	void parse(KXmlAttribute &attribute);
	void parseNode(const char *nodeString);
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir;
	}
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	const char *getType() override
	{
		return "mserver";
	}
#ifdef ENABLE_MSERVER_ICP
	void setIcp(const char *icp_name);
#endif
	void setErrorTryTime(int max_error_count,int errorTryTime);
	static void nodeForm(KWStream &s, const KString &name,KMultiAcserver *as,unsigned nodeIndex);
	bool delNode(int nodeIndex);
	bool isChanged(KPoolableRedirect *rd) override;
	void buildVNode();
	void set_proto(Proto_t proto) override;
	bool ip_hash;
	bool url_hash;
	bool cookie_stick;
	int max_error_count;
protected:

private:
	void removeAllNode();
	void init();
	bool addNode(const KXmlAttribute&attr);
	bool addNode(KXmlAttribute&attr, char *self_ip);
	void addNode(KSockPoolHelper *sockHelper);
	uint16_t getNodeIndex(KHttpRequest *rq,int *set_cookie_stick);
	int getCookieStick(const char *attr, size_t len, const char *cookie_stick_name);
	KSockPoolHelper *getIndexNode(int index);
	KSockPoolHelper *nextActiveNode(KSockPoolHelper *node,unsigned short &index);
	void enableAllServer();	
	//backup node list
	std::vector<KSockPoolHelper *> bnodes;
	std::vector<KSockPoolHelper *> vnodes;
	KSockPoolHelper *nodes;
#ifdef ENABLE_MSERVER_ICP
	KPoolableRedirect *icp;
#endif
	int nodesCount;
	int errorTryTime;
	KMutex lock;
};
#endif /* KMULTIACSERVER_H_ */
