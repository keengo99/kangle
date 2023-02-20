/*
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
#ifndef KPERIPACL_H_
#define KPERIPACL_H_
#include<string>
#include<map>
#include "ksocket.h"
#include "KAcl.h"
#include "KXml.h"
#include "utils.h"
class KPerIpAcl;
struct KPerIpCallBackData
{
	char *ip;
	KPerIpAcl *mark;
};
void per_ip_mark_call_back(void *data);
class KPerIpAcl: public KAcl {
public:
	KPerIpAcl() {
		max_per_ip = 0;
	}
	virtual ~KPerIpAcl() {
	}
	bool supportRuntime() override
	{
		return true;
	}
	void addCallBack(KHttpRequest *rq,const char *ip)
	{
		KPerIpCallBackData *cd = new KPerIpCallBackData;
		cd->ip = strdup(ip);
		cd->mark = this;
		addRef();
		rq->registerConnectCleanHook(per_ip_mark_call_back,cd);
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		const char *ip = rq->getClientIp();
		std::map<char *, unsigned,lessp>::iterator it_ip;
		bool matched = false;
		ip_lock.Lock();
		it_ip = ip_map.find((char *)ip);
		if (it_ip == ip_map.end()) {
			if (1 > max_per_ip) {
				matched = true;
			} else {				
				ip_map.insert(std::pair<char *, unsigned>(strdup(ip), 1));
				addCallBack(rq,ip);
			}
			//printf("+max_per_ip=1\n");
		} else if ((*it_ip).second >= max_per_ip) {
			matched = true;
			//printf("deny max_per_ip=%d,refs=%d\n",(*it_ip).second,getRef());
		} else {		
			(*it_ip).second++;
			addCallBack(rq,ip);
			//printf("+max_per_ip=%d\n",(*it_ip).second);
		}
		ip_lock.Unlock();
		return matched;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << ">" << max_per_ip;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override
	{
		max_per_ip = atoi(attribute["max"].c_str());
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		KPerIpAcl *mark = (KPerIpAcl *) (model);
		s << "><input type=text name=max value='" << (mark ? mark->max_per_ip : 0) << "'>";
		return s.str();
	}
	void callBack(KPerIpCallBackData *data);
	KAcl *new_instance() override {
		return new KPerIpAcl();
	}
	const char *getName() override {
		return "per_ip";
	}
public:
	bool startElement(KXmlContext *context) override {
		max_per_ip = atoi(context->attribute["max"].c_str());
		return true;
	}
	void buildXML(std::stringstream &s) override {
		s << " max='" << max_per_ip << "'>";
	}

private:
	KMutex ip_lock;
	std::map<char *, unsigned,lessp> ip_map;
	unsigned max_per_ip;
};

#endif /*KPERHOSTMARK_H_*/
