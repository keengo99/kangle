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
class KMultiAcserver: public KPoolableRedirect {
public:
	KMultiAcserver();
	KMultiAcserver(KSockPoolHelper *nodes);
	virtual ~KMultiAcserver();
public:
	KUpstream* GetUpstream(KHttpRequest* rq);
	unsigned getPoolSize();
	bool editNode(std::map<std::string,std::string> &attr);
	void buildXML(std::stringstream &s);
	void buildAttribute(std::stringstream &s);
	static void baseHtml(KMultiAcserver *mserver,std::stringstream &s);
	void getHtml(std::stringstream &s);
	void getNodeInfo(std::stringstream &s);
	friend class KAcserverManager;
	static std::string form(KMultiAcserver *mserver);
	void parse(std::map<std::string,std::string> &attribute);
	void parseNode(const char *nodeString);
	const char *getType()
	{
		return "mserver";
	}
	//{{ent
#ifdef ENABLE_MSERVER_ICP
	void setIcp(const char *icp_name);
#endif//}}
	void setErrorTryTime(int max_error_count,int errorTryTime);
	static std::string nodeForm(std::string name,KMultiAcserver *as,unsigned nodeIndex);
	bool delNode(int nodeIndex);
	bool isChanged(KPoolableRedirect *rd);
	void buildVNode();
	bool ip_hash;
	bool url_hash;
	bool cookie_stick;
	int max_error_count;
private:
	void removeAllNode();
	void init();
	bool addNode(std::map<std::string, std::string> &attr);
	bool addNode(std::map<std::string, std::string> &attr, char *self_ip);
	void addNode(KSockPoolHelper *sockHelper);
	uint16_t getNodeIndex(KHttpRequest *rq,int *set_cookie_stick);
	int getCookieStick(const char *attr,const char *cookie_stick_name);
	KSockPoolHelper *getIndexNode(int index);
	KSockPoolHelper *nextActiveNode(KSockPoolHelper *node,unsigned short &index);
	void enableAllServer();	
	//backup node list
	std::vector<KSockPoolHelper *> bnodes;
	std::vector<KSockPoolHelper *> vnodes;
	KSockPoolHelper *nodes;
	//{{ent
#ifdef ENABLE_MSERVER_ICP
	KPoolableRedirect *icp;
#endif//}}
	int nodesCount;
	int errorTryTime;
	KMutex lock;
};
#endif /* KMULTIACSERVER_H_ */
