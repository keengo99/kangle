/*
 * KVirtualProcess.h
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */

#ifndef KVIRTUALPROCESS_H
#define KVIRTUALPROCESS_H
#include <sstream>
#include "global.h"
#include "KExtendProgram.h"
#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "KHttpRequest.h"
#include "KProcess.h"
#include "kfiber.h"
#include "time_utils.h"
#include "HttpFiber.h"


enum VProcess_Status
{
	VProcess_Poweroff,
	VProcess_Inprogress,
	VProcess_Poweron,
	VProcess_Close
};
struct KVirtualHostProcessQueue
{
	kselector* selector;
	kfiber* fiber;
	KVirtualHostProcessQueue* next;
};
class KListenPipeStream;
/*
 * 一个虚拟主机进程模型
 */
class KVirtualHostProcess : public KPoolableSocketContainer
{
public:
	KVirtualHostProcess() {
		//closed = false;
		lastActive = kgl_current_sec;
		idleTime = 0;
		max_request = 0;
		max_error_count = 0;
		error_count = 0;
		status = VProcess_Poweroff;
		lastCheckTime = 0;
		queue = NULL;
	}
	virtual ~KVirtualHostProcess() {
		assert(queue == NULL);
		killProcess(0);
	}
	void isBad(KUpstream* st, BadStage stage)
	{
		if (stage == BadStage_Connect || stage == BadStage_TrySend) {
			error_count++;
			if (kgl_current_sec - lastCheckTime > 5) {
				lastCheckTime = kgl_current_sec;
				if (!isProcessActive() || (max_error_count > 0 && error_count >= max_error_count)) {
					clean();
					status = VProcess_Close;
					//reset the error_count
					error_count = 0;
					klog(KLOG_ERR, "restart the virtual process error_count=%d\n", error_count);
				}
			}
		}
	}
	void isGood(KUpstream* st)
	{
		//reset the error_count
		error_count = 0;
	}
	kconnection* try_connect(sockaddr_i* addr);
	virtual KUpstream* GetUpstream(KHttpRequest* rq, KExtendProgram* rd);
	KUpstream* Connect(KHttpRequest* rq)
	{
		lastActive = kgl_current_sec;
		KUpstream* ps = get_pool_socket();
		if (ps) {
			return ps;
		}
#ifdef KSOCKET_UNIX
		if (!unix_path.empty()) {
			struct sockaddr_un un_addr;
			ksocket_unix_addr(unix_path.c_str(), &un_addr);
			SOCKET fd = ksocket_half_connect((sockaddr_i*)&un_addr, NULL, 0);
			if (!ksocket_opened(fd)) {
				return NULL;
			}
			kconnection* cn = kconnection_new(&addr);
			cn->st.fd = fd;
			return new_upstream(cn);
		}
#endif
		kconnection* cn = kfiber_net_open(&addr);
		if (kfiber_net_connect(cn, NULL, 0) == 0) {
			return new_upstream(cn);
		}
		kfiber_net_close(cn);
		return NULL;
	}
	virtual KUpstream* PowerResult(KHttpRequest* rq, KPipeStream* st) = 0;
	KPipeStream* Power(KHttpRequest* rq, KExtendProgram* rd);
	virtual KPipeStream* PowerThread(KVirtualHost* vh, KExtendProgram* rd) = 0;
	virtual void getProcessInfo(const USER_T& user, const std::string& name,
		std::stringstream& s, int& count) {
	}
	//{{ent
#ifdef ENABLE_ADPP
	virtual void flushCpuUsage(const std::string& user, const std::string& name, ULONG64 cpuTime) {

	}
#endif
	virtual int getPortMap(KVirtualHost* vh, KExtendProgram* rd, std::string name)
	{
		return -1;
	}
	//}}
	virtual bool canDestroy(time_t nowTime)
	{
		if (idleTime > 0 && nowTime - lastActive > idleTime) {
			return true;
		}
		return false;
	}
	/*
	杀掉指定进程，pid=0，杀掉全部进程。
	返回true,则表示全部进程已杀掉，
	返回false,则表示还有进程(针对多进程)
	*/
	virtual bool killProcess(int pid) {
		return false;
	}
	/*
	 标识是否已经结束
	 */
	int idleTime;
	VProcess_Status status;
	pthread_t workThread;
	KMutex lock;
	int error_count;
	int max_error_count;
	unsigned max_request;
	//u_short port;
	union
	{
		sockaddr_i addr;
#ifdef KSOCKET_UNIX
		struct sockaddr_un unix_addr;
#endif
	};
	KVirtualHostProcessQueue* queue;
	time_t lastCheckTime;
protected:
	time_t lastActive;
	virtual bool isProcessActive()
	{
		return true;
	}
#ifdef KSOCKET_UNIX
	std::string unix_path;
#endif
private:
	void NoticePowerResult(bool result);
	void AddQueue()
	{
		KVirtualHostProcessQueue* qi = new KVirtualHostProcessQueue();
		qi->selector = kgl_get_tls_selector();
		qi->fiber = kfiber_self();
		qi->next = queue;
		queue = qi;
	}
};
struct VProcessPowerParam
{
	KVirtualHost* vh;
	KVirtualHostProcess* process;
	KExtendProgram* rd;
	KPipeStream* st;
	kselector* selector;
	kfiber* fiber;
};
void getProcessInfo(const USER_T& user, const std::string& name, KProcess* process, KPoolableSocketContainer* ps, std::stringstream& s);
KTHREAD_FUNCTION VProcessPowerWorker(void* param);
#endif /* KVIRTUALPROCESS_H_ */
