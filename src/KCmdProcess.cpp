/*
 * KCmdProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include "KCmdProcess.h"
#include "lang.h"
#include "KCmdPoolableRedirect.h"
#include "KAsyncFetchObject.h"
#ifdef ENABLE_VH_RUN_AS
//启动进程工作线程
KSPCmdProcess::KSPCmdProcess() {
	st = NULL;
}
KSPCmdProcess::~KSPCmdProcess() {
	if (st) {
		delete st;
	}
}
KUpstream *KSPCmdProcess::PowerResult(KHttpRequest *rq, KPipeStream* st2)
{
	stLock.Lock();
	if (st) {
		delete st;
	}
	st = static_cast<KListenPipeStream *>(st2);
	//这里把端口号保存，下次连接时就不用对stLock加锁了。
#ifdef KSOCKET_UNIX
	if (unix_path.empty()) {
#endif
		ksocket_getaddr("127.0.0.1", st->getPort(), AF_UNSPEC, AI_NUMERICHOST, &addr);
#ifdef KSOCKET_UNIX
	}
#endif
	stLock.Unlock();
	kconnection* cn = TryConnect(&addr);
	if (cn != NULL) {
		return new_upstream(cn);
	}
	return NULL;
}
KPipeStream *KSPCmdProcess::PowerThread(KVirtualHost *vh,KExtendProgram *erd)
{
	KCmdPoolableRedirect *rd = static_cast<KCmdPoolableRedirect *> (erd);
	KListenPipeStream *st2 = new KListenPipeStream;
	if (rd->port > 0) {
		st2->setPort(rd->port);
	}
#ifdef KSOCKET_UNIX
	unix_path.clear();
#endif
	bool result = rd->Exec(vh, st2, false);
	if (!result) {
		delete st2;
		return NULL;
	}
	return st2;
}
KMPCmdProcess::KMPCmdProcess()
{
	freeProcessList = new KSingleListenPipeStream;
	busyProcessList = new KSingleListenPipeStream;
	klist_init(freeProcessList);
	klist_init(busyProcessList);
}
KMPCmdProcess::~KMPCmdProcess()
{
	KSingleListenPipeStream *st;
	for (;;) {
		st = klist_head(freeProcessList);
		if (st == freeProcessList) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	for (;;) {
		st = klist_head(busyProcessList);
		if (st == busyProcessList) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	delete freeProcessList;
	delete busyProcessList;
}
void KMPCmdProcess::set_tcp(bool tcp)
{
	KPoolableSocketContainer::set_tcp(tcp);
	stLock.Lock();
	KSingleListenPipeStream* st;
	klist_foreach(st, busyProcessList) {
		st->set_tcp(tcp);
	}
	klist_foreach(st, freeProcessList) {
		st->set_tcp(tcp);
	}
	stLock.Unlock();
}
KUpstream *KMPCmdProcess::PowerResult(KHttpRequest *rq, KPipeStream* st2)
{
	KSingleListenPipeStream* st = static_cast<KSingleListenPipeStream*>(st2);
	st->vprocess = this;
	addRef();
	stLock.Lock();
	st->set_tcp(tcp);
	klist_append(busyProcessList, st);
	stLock.Unlock();
	//这里把端口号保存，下次连接时就不用对stLock加锁了。
#ifdef KSOCKET_UNIX
	if (!st->unix_path.empty()) {
		ksocket_unix_addr(st->unix_path.c_str(),&st->un_addr);
	} else {
#endif
		int port = st->getPort();
		ksocket_getaddr("127.0.0.1", port, AF_UNSPEC, AI_NUMERICHOST, &st->addr);
#ifdef KSOCKET_UNIX
	}
#endif
	kconnection *cn = TryConnect(&st->addr);
	if (cn != NULL) {
		KUpstream *us = new KTcpUpstream(cn);
		st->bind(us);
		return us;
	}
	return NULL;
}
KUpstream* KMPCmdProcess::get_connection(KHttpRequest* rq, KSingleListenPipeStream* sp)
{
	addRef();
	KUpstream* socket = sp->getConnection(rq);
	if (socket == NULL) {
		sp->killChild();
		gcProcess(sp);
	}
	return socket;
}

KUpstream* KMPCmdProcess::GetUpstream(KHttpRequest* rq, KExtendProgram* rd)
{
	KSingleListenPipeStream* sp = NULL;
	stLock.Lock();
	if (!klist_empty(freeProcessList)) {
		sp = klist_head(freeProcessList);
		klist_remove(sp);
		klist_append(busyProcessList, sp);
	}
	stLock.Unlock();
	if (sp) {
		return get_connection(rq, sp);
	}
	KCmdPoolableRedirect* cmd = static_cast<KCmdPoolableRedirect*>(rd);
	cmd->LockCommand();
	KPipeStream *st = Power(rq, rd);
	if (st == NULL) {
		cmd->UnlockCommand();
		return NULL;
	}
	KUpstream *us = PowerResult(rq, st);
	cmd->UnlockCommand();
	return us;
}
KPipeStream *KMPCmdProcess::PowerThread(KVirtualHost* vh, KExtendProgram* erd)
{
	KCmdPoolableRedirect *rd = static_cast<KCmdPoolableRedirect *> (erd);
	KSingleListenPipeStream *st = new KSingleListenPipeStream;
#ifdef KSOCKET_UNIX
	if (conf.unix_socket) {
		std::stringstream s;
		s << "/tmp/kangle_" << getpid() << (void *)st << ".sock";
		//s.str().swap(st->unix_path);
		if (!st->listen(s.str().c_str())) {
			delete st;
			return NULL;
		}
	} else {
#endif
		if (!st->listen(0,"127.0.0.1")) {
			delete st;
			return NULL;
		}
#ifdef KSOCKET_UNIX
	}
#endif
	//we need cmdLock and the stLock
	cmdLock.Lock();
	st->setLifeTime(this->lifeTime);
	bool result = rd->Exec(vh, st, !klist_empty(busyProcessList));
	if (!result) {
		cmdLock.Unlock();
		delete st;
		return NULL;
	}
	cmdLock.Unlock();
	return st;
}
void KMPCmdProcess::gcProcess(KSingleListenPipeStream *st)
{
	bool isKilled = st->process.isKilled();
	stLock.Lock();
	klist_remove(st);
	if (!isKilled) {
		KSingleListenPipeStream *head = freeProcessList->next;
		klist_insert(head,st);
	}
	stLock.Unlock();	
	if (isKilled) {
		delete st;
	}
	release();
}
//{{ent
#ifdef ENABLE_ADPP
void KMPCmdProcess::flushCpuUsage(const std::string &user,const std::string &name,ULONG64 cpuTime)
{
	KSingleListenPipeStream *st;
	stLock.Lock();
	klist_foreach (st,busyProcessList) {
		st->process.flushCpuUsage(user,name,cpuTime,conf.process_cpu_usage);
	}
	klist_foreach(st, freeProcessList) {
		st->process.flushCpuUsage(user, name, cpuTime, conf.process_cpu_usage);
	}
	stLock.Unlock();
}
#endif
//}}
void KMPCmdProcess::getProcessInfo(const USER_T &user, const std::string &name,std::stringstream &s,int &count)
{
	KSingleListenPipeStream *st;
	stLock.Lock();
	klist_foreach(st, busyProcessList) {
		count++;
		::getProcessInfo(user, name, &st->process, st, s);
	}
	klist_foreach(st, freeProcessList) {
		count++;
		::getProcessInfo(user, name, &st->process, st, s);
	}
	stLock.Unlock();
}
bool KMPCmdProcess::killProcess(int pid)
{
	KSingleListenPipeStream *st;
	bool successKilled = false;
	stLock.Lock();
	klist_foreach(st, busyProcessList) {
		if (pid == 0) {
			st->killChild();
			st->unlink_unix();
			continue;
		}
		if (st->process.getProcessId() == pid) {
			st->killChild();
			st->unlink_unix();
			successKilled = true;
			break;
		}
	}
	if (!successKilled) {
		klist_foreach(st, freeProcessList) {
			if (pid == 0) {
				st->killChild();
				st->unlink_unix();
				continue;
			}
			if (pid == st->process.getProcessId()) {
				st->killChild();
				st->unlink_unix();
				klist_remove(st);
				delete st;
				break;
			}
		}		
	}
	stLock.Unlock();
	if (pid==0) {
		return true;
	}
	return false;
}
bool KMPCmdProcess::canDestroy(time_t nowTime)
{
	bool result = false;
	KSingleListenPipeStream *st;
	stLock.Lock();
	for (;;) {
		st = klist_end(freeProcessList);
		if (st == freeProcessList) {
			break;
		}
		if (nowTime < idleTime + st->lastActive) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	if(klist_empty(freeProcessList) && klist_empty(busyProcessList)) {
		result = true;
	}
	stLock.Unlock();
	return result;
}
#endif
