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
class KListenPipeStream;
/*
 * һ��������������ģ��
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
	void shutdown() override {
		killProcess(0);
	}
	void health(KUpstream* st, HealthStatus health_status) override
	{
		switch (health_status) {
		case HealthStatus::Err:
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
			break;
		case HealthStatus::Success:
			error_count = 0;
			break;
		default:
			break;
		}
	}
	kconnection* try_connect(sockaddr_i* addr);
	virtual KUpstream* GetUpstream(KHttpRequest* rq, KExtendProgram* rd);
	KUpstream* Connect(KHttpRequest* rq)
	{
		lastActive = kgl_current_sec;
		KUpstream* ps = get_pool_socket(0);
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
	virtual void getProcessInfo(const USER_T& user, const KString& name, KWStream& s, int& count) {
	}
	virtual void dump_process(const KString &user, kgl::serializable* sl) = 0;
	//{{ent
#ifdef ENABLE_ADPP
	virtual void flushCpuUsage(const KString& user, const KString& name, ULONG64 cpuTime) {

	}
#endif
	virtual int getPortMap(KVirtualHost* vh, KExtendProgram* rd, KString name)
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
	ɱ��ָ�����̣�pid=0��ɱ��ȫ�����̡�
	����true,���ʾȫ��������ɱ����
	����false,���ʾ���н���(��Զ����)
	*/
	virtual bool killProcess(int pid) {
		return false;
	}
	/*
	 ��ʶ�Ƿ��Ѿ�����
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
	kfiber_waiter* queue;
	time_t lastCheckTime;
protected:
	time_t lastActive;
	virtual bool isProcessActive()
	{
		return true;
	}
#ifdef KSOCKET_UNIX
	KString unix_path;
#endif
private:
	void NoticePowerResult(bool result);
};
struct VProcessPowerParam
{
	KVirtualHost* vh;
	KVirtualHostProcess* process;
	KExtendProgram* rd;
	KPipeStream* st;
	kfiber* fiber;
};
void getProcessInfo(const USER_T& user, const KString& name, KProcess* process, KPoolableSocketContainer* ps, KWStream& s);
void dump_process_info(KProcess* process, KPoolableSocketContainer* ps, kgl::serializable *sl);
KTHREAD_FUNCTION VProcessPowerWorker(void* param);
#endif /* KVIRTUALPROCESS_H_ */
