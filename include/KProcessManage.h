/*
 * KProcessManage.h
 *
 *  Created on: 2010-8-17
 *      Author: keengo
 */
#ifndef KPROCESSMANAGE_H_
#define KPROCESSMANAGE_H_
#include <string.h>
#include <map>
#include <vector>
#include <list>
#include <time.h>
#include "global.h"
#include "KRedirect.h"
#include "KVirtualHost.h"
#include "ksocket.h"
#include "KMutex.h"
#include "KPipeStream.h"
//#include "api_child.h"
#include "KApiPipeStream.h"
#include "KCmdProcess.h"
#include "KVirtualHostProcess.h"
#include "KApiProcess.h"
class KApiRedirect;
class KApiProcessManage;

/*
 * 进程管理器。
 * 一个用户名对应一个虚拟进程.
 * 一个虚拟进程管理连接和实际进程(可能是一组进程，也可能是一个进程)
 * 具体说有两种模式，一种是SP(单进程，多线程),另一种是MP(多进程,单线程)
 *
 */
class KProcessManage {
public:
	KProcessManage() {

	}
	virtual ~KProcessManage();
	void set_proto(Proto_t proto);
	KUpstream* GetUpstream(KHttpRequest* rq, KExtendProgram* rd);
	void clean();
	void refresh(time_t nowTime);
	void getProcessInfo(std::stringstream &s);
	//{{ent
#ifdef ENABLE_ADPP
	void flushCpuUsage(ULONG64 cpuTime);
#endif
	int getPortMap(KVirtualHost *vh,KExtendProgram *rd,std::string name,int app);
	//}}
	void killAllProcess(KVirtualHost *vh=NULL);
	bool killProcess(const char *user,int pid);
	bool killProcess2(USER_T user,int pid);
	void setName(const char *name) {
		this->name = name;
	}
	std::string param;
protected:
	virtual KVirtualHostProcess *newVirtualProcess(const char *param) = 0;
private:
	KVirtualHostProcess *refsVirtualHostProcess(std::string app,KExtendProgram *rd)
	{
		//printf("refs virtualhost process app [%s]\n",app.c_str());
		KVirtualHostProcess *gc = NULL;
		std::map<USER_T, KVirtualHostProcess *>::iterator it;
		lock.Lock();
		it = pools.find(app);
		if (it != pools.end()) {
			gc = (*it).second;
			if (gc->status == VProcess_Close) {
				debug("process is closed,now recreate it.\n");
				gc->release();
				pools.erase(it);
				gc = NULL;
			} else {
				gc->addRef();
			}
		}
		if (gc == NULL) {
			debug("app [%s] gc is NULL\n",app.c_str());
			gc = newVirtualProcess(param.c_str());
			gc->set_tcp(kangle::is_upstream_tcp(proto));
			assert(gc);
			gc->setLifeTime(rd->life_time);
			//gc->setRefresh(true);
			gc->idleTime = rd->idleTime;
			gc->max_error_count = rd->max_error_count;
			gc->max_request = rd->maxRequest;
			//gc->maxConnect = rd->maxConnect;
			gc->addRef();
			pools.insert(std::pair<USER_T, KVirtualHostProcess *> (app, gc));
		}
		lock.Unlock();
		return gc;
	}
	/*
	 * 进程管理器名称
	 */
	std::string name;
	Proto_t proto;
	KMutex lock;
	std::map<USER_T, KVirtualHostProcess *> pools;
};
/*
 * api的进程管理器
 */
class KApiProcessManage: public KProcessManage {
public:
	KApiProcessManage() {
		set_proto(Proto_fcgi);
	}
	~KApiProcessManage() {

	}
protected:
	KVirtualHostProcess *newVirtualProcess(const char *param) {
		KVirtualHostProcess *ps = new KApiProcess;
		ps->SetParam(param);
		return ps;
	}
};

/*
 * 命令进程管理器
 */
class KCmdProcessManage: public KProcessManage {
public:	
	KCmdProcessManage()
	{
		worker = 0;
	}
	void setWorker(int worker)
	{
		this->worker = worker;
	}
protected:
	KVirtualHostProcess *newVirtualProcess(const char *param) {
		KVirtualHostProcess *ps;
		if (worker==0) {
			ps =  new KMPCmdProcess;
		} else {
			ps = new KSPCmdProcess;
		}
		ps->SetParam(param);
		return ps;
	}

private:
	int worker;
};
extern KApiProcessManage spProcessManage;
#endif /* KPROCESSMANAGE_H_ */
