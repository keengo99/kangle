/*
 * KApiProcess.h
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */

#ifndef KAPIPROCESS_H
#define KAPIPROCESS_H
#include "global.h"
#include "KHttpRequest.h"
#include "KPoolableSocketContainer.h"

#include "KApiRedirect.h"
#include "KApiPipeStream.h"
//#include "api_child.h"
#include "KStream.h"
#include "KExtendProgram.h"
#include "KVirtualHostProcess.h"

class KApiProcess: public KVirtualHostProcess {
public:
	KApiProcess()
	{
		st = NULL;
	}
	virtual ~KApiProcess() {
		if (st) {
			delete st;
		}
	}
	void getProcessInfo(const USER_T &user, const std::string &name,
			std::stringstream &s,int &count) {
		stLock.Lock();
		if (st) {
			count++;
			::getProcessInfo(user,name,&st->process,this,s);
		}
		stLock.Unlock();
	}
	bool killProcess(int pid) {
		stLock.Lock();
		if (st) {
			delete st;
			st = NULL;
		}
		status = VProcess_Close;
		stLock.Unlock();
		return true;
	}
	KPipeStream *PowerThread(KVirtualHost *vh, KExtendProgram *rd) override;
	KUpstream* PowerResult(KHttpRequest* rq, KPipeStream* st2) override;
	//{{ent
#ifdef ENABLE_ADPP
	void flushCpuUsage(const std::string &user,const std::string &name,ULONG64 cpuTime)
	{
		stLock.Lock();
		if (st) {
			st->process.flushCpuUsage(user,name,cpuTime,conf.process_cpu_usage);
		}
		stLock.Unlock();

	}
#endif
	//}}
protected:
	bool isProcessActive()
	{
		bool result = false;
		stLock.Lock();		
		if(st){
			result = st->process.isActive();
		}
		stLock.Unlock();
		return result;
	}
private:
	u_short loadApi(KStream *pst, KHttpRequest *rq, KApiRedirect *rd);
	KApiPipeStream *st;
	KMutex stLock;
};
#endif /* KAPIPROCESS_H_ */
