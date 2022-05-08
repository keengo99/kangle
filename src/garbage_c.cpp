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
#include "global.h"
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <time.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include "cache.h"
#include "utils.h"
#include "log.h"
#include "extern.h"

#include "utils.h"
#include "kthread.h"
#include "lib.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "kselector_manager.h"
#include "kmalloc.h"
#include "KHttpObjectHash.h"
#include "KSimulateRequest.h"
#include "KVirtualHostManage.h"
#include "KProcessManage.h"
#include "KProcess.h"
#include "KLogManage.h"
#include "KHttpServerParser.h"
#include "kmalloc.h"
#include "KHttpDigestAuth.h"
#include "KObjectList.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KCdnContainer.h"
#include "KWriteBackManager.h"
#include "kaddr.h"
#include "kmd5.h"
#include "KTimer.h"
struct kgl_gc_service
{
	void (*flush)(void*, time_t);
	void* arg;
	kgl_gc_service* next;
};
void list_all_malloc();
using namespace std;

bool dump_memory_object = false;
volatile int stop_service_sig = 0;
volatile bool autoupdate_thread_started = false;
kgl_gc_service* gc_service = NULL;

//{{ent
KTHREAD_FUNCTION crash_report_thread(void* arg);
//}}
#ifdef ENABLE_VH_FLOW
volatile bool flushFlowFlag = false;
time_t lastFlushFlowTime = time(NULL);
#endif
void register_gc_service(void(*flush)(void*, time_t), void* arg) {
	kgl_gc_service* gs = new kgl_gc_service;
	gs->flush = flush;
	gs->arg = arg;
	gs->next = gc_service;
	gc_service = gs;
}
std::string md5sum(FILE* fp)
{
	KMD5_CTX context;
	unsigned char digest[17];
	char buf[1024];
	KMD5Init(&context);
	for (;;) {
		int len = (int)fread(buf, 1, (int)sizeof(buf), fp);
		if (len <= 0)
			break;
		KMD5Update(&context, (unsigned char*)buf, len);
		if (len != sizeof(buf)) {
			break;
		}
	}
	KMD5Final(digest, &context);
	for (int i = 0; i < 16; i++) {
		sprintf(buf + 2 * i, "%02x", digest[i]);
	}
	buf[32] = 0;
	return buf;
}
#if 0
void get_cache_size(INT64& total_mem_size, INT64& total_disk_size) {
	cache.getSize(total_mem_size, total_disk_size);
}
#endif
//{{ent
KTHREAD_FUNCTION check_autoupdate(void* param)
{

	KStringBuf s;
	s << conf.path << "bin/autoupdate";
#ifdef _WIN32
	s << ".exe";
#endif
	KStringBuf au_url;
	KStringBuf service;
	INT64 method = (INT64)param;
	const char* action = "down";
	if (method == 0 || method == 1) {
		au_url << "http://autoupdate.kangleweb.net/config.php?";
		au_url << "m=" << method << "&version=" << VERSION << "&type=" << getServerType() << "&f=";
	} else {
#ifdef _WIN32
		service << PROGRAM_NAME;
#else
		service << conf.program << " --reboot";
#endif
		action = "install";
	}
	char* args[] = {
		s.getString(),
		(char*)conf.path.c_str(),
		(method < 2 ? au_url.getString() : service.getString()),
		(char*)action,
		NULL
	};
	if (!startProcessWork(NULL, args, NULL)) {
		klog(KLOG_ERR, "cann't create autoupdate process\n");
	}
	autoupdate_thread_started = false;
	KTHREAD_RETURN;
}
//}}
void flush_mem_cache(int64_t last_msec) {
	INT64 disk_cache = 0;
	bool disk_is_radio = false;
#ifdef ENABLE_DISK_CACHE
	disk_cache = conf.disk_cache;
	disk_is_radio = conf.disk_cache_is_radio;
#endif
	cache.flush(last_msec, conf.mem_cache, disk_cache, disk_is_radio);
}
/*
void reloadVirtualHostConfig() {
#ifndef HTTP_PROXY
	vhd.clear();
	conf.gvm->updateAllVirtualHost();
	conf.gvm->load();
	conf.gvm->checkAllVirtualHost();
#endif
}
*/
KTHREAD_FUNCTION time_thread(void* arg) {
#ifdef ENABLE_SIMULATE_HTTP
	kassert(test_simulate_request());
#endif
	//assert(test_timer());
	unsigned i = rand();
#ifdef MALLOCDEBUG
	void start_hook_alloc();
	bool test();
	if (conf.mallocdebug) {
		start_hook_alloc();
	}
	assert(test());
#endif
	time_t nowTime;
	INT64 now_msec;
	INT64 last_msec = katom_get64((void*)&kgl_current_msec);
	for (;;) {
		i++;
		now_msec = katom_get64((void*)&kgl_current_msec);
		int past_time = (int)(now_msec - last_msec);
		int sleep_msec = GC_SLEEP_MSEC - past_time;
		if (sleep_msec > GC_SLEEP_MSEC) {
			sleep_msec = GC_SLEEP_MSEC;
		}
		last_msec = now_msec;
		//printf("last_msec=[%lld] now_msec=[%lld] past_time=[%d] sleep_msec=[%d]\n", last_msec,now_msec,past_time,sleep_msec);
		if (sleep_msec > 0) {
			my_msleep(sleep_msec);
			last_msec += sleep_msec;
		}
		nowTime = (time_t)(last_msec / 1000);
#ifdef MALLOCDEBUG
		if (quit_program_flag == PROGRAM_QUIT_SHUTDOWN) {
			break;
		}
#endif
		flush_mem_cache(last_msec);
#ifdef ENABLE_VH_FLOW
		//自动刷新流量
		if (conf.flush_flow_time > 0 && nowTime - lastFlushFlowTime > conf.flush_flow_time) {
			lastFlushFlowTime = nowTime;
			flushFlowFlag = true;
		}
		if (flushFlowFlag) {
			conf.gvm->dumpFlow();
			flushFlowFlag = false;
		}
#endif
		//{{ent
#ifndef HTTP_PROXY
//}}
		spProcessManage.refresh(nowTime);
		conf.gam->refreshCmd(nowTime);
		//{{ent
#endif
#ifdef ENABLE_ADPP
		if (conf.process_cpu_usage > 0) {
			ULONG64 cpuTime = KProcess::getCpuTime();
			spProcessManage.flushCpuUsage(cpuTime);
			conf.gam->flushCpuUsage(cpuTime);
		}
#endif
		//}}
#ifdef MALLOCDEBUG
		if (dump_memory_object) {
			dump_memory_leak(0, -1);
			dump_memory_object = false;
		}
#endif
		if (i % 6 == 0) {
			accessLogger.checkRotate(nowTime);
			errorLogger.checkRotate(nowTime);
			logManage.checkRotate(nowTime);
			kthread_flush(conf.min_free_thread);
#ifdef ENABLE_DIGEST_AUTH
			KHttpDigestAuth::flushSession(nowTime);
#endif		
			kgl_flush_addr_cache(nowTime);
		}
		server_container->flush(kgl_current_sec);
		if (vhd.isLoad() && !vhd.isSuccss()) {
			klog(KLOG_ERR, "vh database last status is failed.try again.\n");
			std::string errMsg;
			if (!vhd.loadVirtualHost(conf.gvm, errMsg)) {
				klog(KLOG_ERR, "Cann't load VirtualHost[%s]\n", errMsg.c_str());
			}
		}
#ifdef ENABLE_DISK_CACHE
		scan_disk_cache();
		if (dci && !cache.is_disk_shutdown()) {
			dci->start(ci_flush, NULL);
		}
#endif
		kgl_gc_service* gs = gc_service;
		while (gs) {
			gs->flush(gs->arg, nowTime);
			gs = gs->next;
		}
	}
	while (gc_service) {
		kgl_gc_service* next = gc_service->next;
		delete gc_service;
		gc_service = next;
	}
	KTHREAD_RETURN;
}

