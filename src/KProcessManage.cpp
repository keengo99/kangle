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
static const int life_time = 60;
using namespace std;


KProcessManage::~KProcessManage() {
	clean();
}
void KProcessManage::set_proto(Proto_t proto)
{
	lock.Lock();
	this->proto = proto;
	std::map<USER_T, KVirtualHostProcess*>::iterator it;
	for (it = pools.begin(); it != pools.end(); it++) {
		(*it).second->set_tcp(kangle::is_upstream_tcp(proto));
	}
	lock.Unlock();
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
	auto svh = kangle::get_virtual_host(rq);
	if (!svh) {
		return nullptr;
	}
	KVirtualHostProcess* gc = refsVirtualHostProcess(svh->vh->getApp(rq), rd);
	if (!gc) {
		return nullptr;
	}
	KUpstream* us = gc->GetUpstream(rq, rd);
	gc->release();
	return us;
}
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
int KProcessManage::getPortMap(KVirtualHost *vh,KExtendProgram *rd,KString name,int app)
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

void KProcessManage::getProcessInfo(KWStream&s) {
	int count = 0;
	KStringBuf t;
	lock.Lock();
	if (pools.size() == 0) {
		lock.Unlock();
		return;
	}
	for (auto it = pools.begin(); it != pools.end(); it++) {
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
	lock.Lock();
	if (vh) {
		for(size_t i=0;i<vh->apps.size();i++){
			auto it = pools.find(vh->apps[i]);
			if (it!=pools.end()) {
				(*it).second->killProcess(0);
				(*it).second->release();
				pools.erase(it);
			}
		}
	} else {
		for (auto it = pools.begin(); it != pools.end(); it++) {
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

bool KProcessManage::killProcess2(const KString &user,int pid) {
	bool result = false;
	lock.Lock();
	auto it = pools.find(user);
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

