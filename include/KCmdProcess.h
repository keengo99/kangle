/*
 * KCmdProcess.h
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */

#ifndef KCMDPROCESS_H_
#define KCMDPROCESS_H_

#include "KVirtualHostProcess.h"
#include "klist.h"
#include "KListenPipeStream.h"
#include "kselector_manager.h"
#include "KTcpUpstream.h"
#include "kfiber.h"
/*
���߳��������
*/
class KSPCmdProcess: public KVirtualHostProcess {
public:
	KSPCmdProcess();
	~KSPCmdProcess();
	KUpstream *PowerResult(KHttpRequest* rq, KPipeStream* st) override;
	KPipeStream * PowerThread(KVirtualHost *vh,KExtendProgram *rd) override;
	void dump_process(const KString& app, kgl::serializable* sl) override {
		KLocker lock(&stLock);
		if (!st) {
			return;
		}
		auto sl_process = sl->add_obj_array("process");
		if (!sl_process) {
			return;
		}
		sl_process->add("app", app);
		dump_process_info(&st->process, this, sl_process);
	}
	void getProcessInfo(const USER_T &user, const KString &name, KWStream &s,int &count) override
	{
		stLock.Lock();
		if (st) {
			count++;
			::getProcessInfo(user,name,&st->process,this,s);
		}
		stLock.Unlock();
	}
	bool killProcess(int pid) override {
		stLock.Lock();
		if (st) {
			delete st;
			st = NULL;
		}
		status = VProcess_Close;
		stLock.Unlock();
		return true;
	}
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
#endif//}}
protected:
	bool isProcessActive() override
	{
		bool result = false;
		stLock.Lock();		
		if(st){
			result = st->process.isActive();
		}
		stLock.Unlock();
		return result;
	}
	KMutex stLock;
	KListenPipeStream *st;
};
class KSingleListenPipeStream;
//�����������չ
class KMPCmdProcess: public KVirtualHostProcess {
public:
	KMPCmdProcess();
	~KMPCmdProcess();
	KUpstream* GetUpstream(KHttpRequest* rq, KExtendProgram* rd) override;
	//kev_result handleRequest(KHttpRequest *rq,KExtendProgram *rd, KAsyncFetchObject *fo);
	void dump_process(const KString &user, kgl::serializable* sl) override;
	void getProcessInfo(const USER_T &user, const KString&name, KWStream &s,int &count) override;
	bool killProcess(int pid) override;
	//KTcpUpstream *poweron(KVirtualHost *vh,KExtendProgram *erd, bool &success);
	KPipeStream*PowerThread(KVirtualHost* vh, KExtendProgram* rd) override;
	KUpstream *PowerResult(KHttpRequest* rq, KPipeStream* st) override;
	void gcProcess(KSingleListenPipeStream *st);
	bool isMultiProcess()
	{
		return true;
	}
	void set_tcp(bool tcp) override;
#ifdef ENABLE_ADPP
	void flushCpuUsage(const KString&user,const KString&name,ULONG64 cpuTime) override;
#endif
	bool canDestroy(time_t nowTime) override;
private:
	KUpstream* get_connection(KHttpRequest *rq, KSingleListenPipeStream* sp);
	KSingleListenPipeStream *freeProcessList;
	KSingleListenPipeStream *busyProcessList;
	KMutex stLock;
	KMutex cmdLock;
};
class KSingleListenPipeStream : public KListenPipeStream,public KPoolableSocketContainer
{
public:
	KSingleListenPipeStream()
	{
		lastActive = kgl_current_sec;
	}
	~KSingleListenPipeStream()
	{
		unlink_unix();
	}
	void shutdown() override {
		refresh(0);
		vprocess->shutdown();
	}
	KUpstream *getConnection(KHttpRequest *rq)
	{
		lastActive = kgl_current_sec;
		KUpstream*st = KPoolableSocketContainer::get_pool_socket(0);
		if (st) {
			return st;
		}
		kconnection* cn = kfiber_net_open(&addr);
		if (kfiber_net_connect(cn, NULL, 0) != 0) {
			kfiber_net_close(cn);
			return NULL;
		}
		st = new_upstream(cn);		
		if (st) {
			bind(st);
		}
		return st;
	}
	void gcSocket(KUpstream* st, int life_time) override
	{
		//ʹ����KTsUpstream��windows�¶�iocp��Ҳ����ʹ�ó�����.
		KPoolableSocketContainer::gcSocket(st, life_time);
		kassert(vprocess!=NULL);
		vprocess->gcProcess(this);
	}
	void health(KUpstream *st,HealthStatus stage) override
	{
		switch (stage) {
		case HealthStatus::Err:
			process.kill();
			break;
		default:
			break;
		}
	}
	friend class KMPCmdProcess;
private:
	//KUpstream *socket;
	KMPCmdProcess *vprocess;
#ifdef KSOCKET_UNIX
	union {
		sockaddr_i addr;
		struct sockaddr_un un_addr;
	};
#else
	sockaddr_i addr;
#endif
	time_t lastActive;
	KSingleListenPipeStream *next;
	KSingleListenPipeStream *prev;
};

#endif /* KCMDPROCESS_H_ */
