/*
 * KProcessManage.cpp
 *
 *  Created on: 2010-8-17
 *      Author: keengo
 */

#include "KProcessManage.h"
#include "KApiRedirect.h"
#include "KApiPipeStream.h"
#include "http.h"
#include "api_child.h"
#include "extern.h"
#include "lang.h"
#include "kthread.h"
#include "KAsyncFetchObject.h"
#include "kselector.h"

KApiProcessManage spProcessManage;
static const int lifeTime = 60;
using namespace std;


KProcessManage::~KProcessManage() {
	clean();
}
void KProcessManage::clean()
{
	lock.Lock();
	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	for (it = pools.begin(); it != pools.end(); it++) {
		(*it).second->release();
	}
	pools.clear();
	lock.Unlock();
}
KUpstream* KProcessManage::GetUpstream(KHttpRequest* rq, KExtendProgram* rd)
{
	KVirtualHostProcess* gc = refsVirtualHostProcess(rq->get_virtual_host()->vh->getApp(rq), rd);
	if (gc == NULL) {
		return NULL;
	}
	KUpstream* us = gc->GetUpstream(rq, rd);
	gc->release();
	return us;
}
#if 0
kev_result KProcessManage::connect(KHttpRequest *rq, KExtendProgram *rd, KAsyncFetchObject *fo) {
	KVirtualHostProcess *gc = refsVirtualHostProcess(rq->svh->vh->getApp(rq),rd);
	if (gc == NULL) {
		return fo->connectCallBack(rq,NULL);
	}
	kev_result ret = gc->handleRequest(rq,rd,fo);
	gc->release();
	return ret;
}
#endif
//{{ent
#ifdef ENABLE_ADPP
void KProcessManage::flushCpuUsage(ULONG64 cpuTime) {
	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	lock.Lock();
	for (it = pools.begin(); it != pools.end(); it++) {
		(*it).second->flushCpuUsage((*it).first,name,cpuTime);
	}
	lock.Unlock();
}
#endif
int KProcessManage::getPortMap(KVirtualHost *vh,KExtendProgram *rd,std::string name,int app)
{
	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	if(app<0 || app>(int)vh->apps.size()){
		return -1;
	}
	KVirtualHostProcess *gc = refsVirtualHostProcess(vh->apps[app],rd);
	if (gc==NULL) {
		return -1;
	}
	int port = gc->getPortMap(vh,rd,name);
	gc->release();
	return port;
}
//}}
void KProcessManage::refresh(time_t nowTime) {

	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	std::map<USER_T, KVirtualHostProcess *>::iterator it_next;
	lock.Lock();
	for (it = pools.begin(); it != pools.end();) {
		if ((*it).second->canDestroy(nowTime)) {
			(*it).second->release();
			it_next = it;
			it++;
			pools.erase(it_next);
		} else {
			(*it).second->refresh(nowTime);
			it++;
		}
	}
	lock.Unlock();

}

void KProcessManage::getProcessInfo(std::stringstream &s) {

	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	int count = 0;
	std::stringstream t;
	lock.Lock();
	if (pools.size() == 0) {
		lock.Unlock();
		return;
	}
	for (it = pools.begin(); it != pools.end(); it++) {
		(*it).second->getProcessInfo((*it).first, name, t,count);
	}
	lock.Unlock();
	s << name << "(" << count << ")<br><table border=1><tr><td>"
			<< LANG_OPERATOR << "</td><td>" << klang["app"] << "</td><td>"
			<< klang["pid"] << "</td>";
//{{ent
#ifdef ENABLE_ADPP
	if (conf.process_cpu_usage > 0) {
		s << "<td>cpu%</td>";
		s << "<td>" << klang["priority"] << "</td>";
	}
#endif
//}}
	s << "<td>" << klang["lang_sock_refs_size"] << "</td><td>" << klang["lang_sock_pool_size"] << "</td>";
	s << "<td>" << LANG_TOTAL_RUNING << "</td>";
	s << "</tr>\n";
	s << t.str();
	s << "</table>";
}
void KProcessManage::killAllProcess(KVirtualHost *vh) {
	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	lock.Lock();
	if (vh) {
		for(size_t i=0;i<vh->apps.size();i++){
			it = pools.find(vh->apps[i]);
			if (it!=pools.end()) {
				(*it).second->killProcess(0);
				(*it).second->release();
				pools.erase(it);
			}
		}
	} else {
		for (it = pools.begin(); it != pools.end(); it++) {
			(*it).second->killProcess(0);
			(*it).second->release();
		}
		pools.clear();
	}
	lock.Unlock();
}
bool KProcessManage::killProcess(const char * user,int pid) {
//#ifdef _WIN32
	return killProcess2(user,pid);
//#else
//	return killProcess2(atoi(user),pid);
//#endif
}

bool KProcessManage::killProcess2(USER_T user,int pid) {
	bool result = false;
	lock.Lock();
	std::map<USER_T, KVirtualHostProcess *>::iterator it;
	it = pools.find(user);
	if (it != pools.end()) {
		result = (*it).second->killProcess(pid);
		if(result){
			(*it).second->release();
			pools.erase(it);
		}
	}
	lock.Unlock();
	return result;
}

