/*
 * KApiProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include "KApiProcess.h"
#include "kmalloc.h"
#include "lang.h"
KPipeStream* KApiProcess::PowerThread(KVirtualHost* vh, KExtendProgram* erd)
{
	bool unix_socket = false;
#ifdef KSOCKET_UNIX
	unix_socket = conf.unix_socket;
#endif
	KApiRedirect* rd = static_cast<KApiRedirect*> (erd);
	KApiPipeStream* st = new KApiPipeStream;
	if (!rd->createProcess(vh, st) || !st->init(vh, WORK_TYPE_SP)
		|| !st->listen(0, unix_socket)) {
		delete st;
		klog(KLOG_ERR, "cann't create api pipe stream\n");
		return NULL;
	}
	return st;
}
KUpstream* KApiProcess::PowerResult(KHttpRequest* rq, KPipeStream* st2)
{
	stLock.Lock();
	if (st) {
		delete st;
	}
	st = static_cast<KApiPipeStream*>(st2);
	//这里把端口号保存，下次连接时就不用对stLock加锁了。
#ifdef KSOCKET_UNIX	
	if (st->is_unix_socket()) {
		KStringBuf s;
		s << "/tmp/extworker." << st->info.port << ".sock";
		s.str().swap(unix_path);
	} else {
#endif
		if (!ksocket_getaddr("127.0.0.1", st->info.port, AF_UNSPEC, AI_NUMERICHOST, &addr)) {
			klog(KLOG_ERR, "cann't get 127.0.0.1 addr\n");
		}
#ifdef KSOCKET_UNIX	
	}
#endif
	stLock.Unlock();
	kconnection* cn = try_connect(&addr);
	if (cn != NULL) {
		return new_upstream(cn);
	}
	return NULL;
}
