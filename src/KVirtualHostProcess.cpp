/*
 * KVirtualProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include <list>
#include "KVirtualHostProcess.h"
#include "KAsyncFetchObject.h"
#include "KCmdPoolableRedirect.h"
#include "lang.h"
using namespace std;
//启动进程工作线程
KTHREAD_FUNCTION VProcessPowerWorker(void *param)
{
	VProcessPowerParam *vpp = (VProcessPowerParam *)param;
	assert(vpp);
	KVirtualHostProcess *process = vpp->process;
	assert(process);
	KVirtualHost *vh = vpp->vh;
	vpp->st = process->PowerThread(vh, vpp->rd);
	kfiber_wakeup2(vpp->selector, vpp->fiber, vpp->process, 0);
	KTHREAD_RETURN;
}
void getProcessInfo(const USER_T &user,const std::string &name,KProcess *process,KPoolableSocketContainer *ps,std::stringstream &s)
{
	//time_t totalTime = time(NULL) - process->startTime;
	s << "<tr>";
	s << "<td>[<a href='/process_kill?name=" << name << "&user=" << user;
	s << "&pid=" << process->getProcessId() << "'>" << klang["kill"] << "</a>]</td>";
	s << "<td>" << user << "</td>";
	s << "<td >" << process->getProcessId() << "</td>";
//{{ent
#ifdef ENABLE_ADPP
	if (conf.process_cpu_usage > 0) {
		s << "<td>" << process->getLastCpuUsage() << "</td>";
		s << "<td ";
		if (process->getPriority() != NORMAL_PRIORITY_CLASS) {
			s << "bgcolor=red";
		}
		s << ">" << klang[(process->getPriority()
				== NORMAL_PRIORITY_CLASS ? "normal" : "low")] << "</td>";
	}
#endif
//}}
	s << "<td>" << (ps->getRef() - 1) << "</td>";
	s << "<td>" << ps->getSize() << "</td>";
	s << "<td>" << (kgl_current_sec - process->getPowerOnTime()) << "</td>";
	s << "</tr>\n";

}
KPipeStream* KVirtualHostProcess::Power(KHttpRequest* rq, KExtendProgram* rd)
{
	VProcessPowerParam param;
	memset(&param, 0, sizeof(param));
	param.vh = rq->svh->vh;
	param.process = this;
	param.rd = rd;
	param.fiber = kfiber_self();
	param.selector = kgl_get_tls_selector();
	if (!kthread_pool_start(VProcessPowerWorker, &param)) {		
		return NULL;
	}
	kfiber_wait(param.process);
	return param.st;
}
kconnection * KVirtualHostProcess::TryConnect(sockaddr_i* addr)
{
	for (int i = 0; i < 10; i++) {
		kconnection* cn = kfiber_net_open(addr);
		if (kfiber_net_connect(cn, NULL, 0) == 0) {
			return cn;
		}
		kfiber_net_close(cn);
		kfiber_msleep(500);
	}
	return NULL;
}
KUpstream* KVirtualHostProcess::GetUpstream(KHttpRequest* rq, KExtendProgram* rd)
{
#ifdef ENABLE_VH_RUN_AS
	if (status == VProcess_Poweron) {
		return Connect(rq);
	}
	KCmdPoolableRedirect* cmd = static_cast<KCmdPoolableRedirect*>(rd);

	lock.Lock();
	if (status == VProcess_Poweron) {
		lock.Unlock();
		return Connect(rq);
	}	
	if (status != VProcess_Inprogress) {
		status = VProcess_Inprogress;
		lock.Unlock();
		KPipeStream* st = Power(rq, rd);
		if (st == NULL) {
			cmd->UnlockCommand();
			NoticePowerResult(false);
			return NULL;
		}
		KUpstream *us = PowerResult(rq, st);
		cmd->UnlockCommand();
		NoticePowerResult(true);
		return us;
	}
	AddQueue();
	lock.Unlock();
	if (kfiber_wait(this) != 0) {
		return NULL;
	}
#endif
	return Connect(rq);
}
void KVirtualHostProcess::NoticePowerResult(bool result)
{
	lock.Lock();
	KVirtualHostProcessQueue* queue = this->queue;
	this->queue = NULL;
	if (result) {
		status = VProcess_Poweron;
	} else {
		status = VProcess_Poweroff;
	}
	lock.Unlock();
	while (queue) {
		KVirtualHostProcessQueue* next = queue->next;
		kfiber_wakeup2(queue->selector, queue->fiber,this, result ? 0 : -1);
		delete queue;
		queue = next;
	}
}
