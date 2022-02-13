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
KPipeStream *KApiProcess::PowerThread(KVirtualHost *vh,KExtendProgram *erd)
{
	return NULL;
#if 0
	bool unix_socket = false;
#ifdef KSOCKET_UNIX
	unix_socket = conf.unix_socket;
#endif
	sp_info pi;
	KApiRedirect *rd = static_cast<KApiRedirect *> (erd);
	stLock.Lock();
	if (st == NULL) {
		st = new KApiPipeStream;
		if (!rd->createProcess(vh, st) || !st->init(vh, WORK_TYPE_SP)
				|| !st->listen(0, &pi,unix_socket)) {
			delete st;
			st = NULL;
			stLock.Unlock();
			success = false;
			return NULL;
		}
	}
	stLock.Unlock();
#ifdef KSOCKET_UNIX	
	if (unix_socket) {
		std::stringstream s;
		s << "/tmp/extworker." << pi.port << ".sock";
		s.str().swap(unix_path);
	} else {
#endif
		if(!ksocket_getaddr("127.0.0.1", pi.port, AF_UNSPEC, AI_NUMERICHOST, &addr)){
			klog(KLOG_ERR,"cann't get 127.0.0.1 addr\n");
			success = false;
			return NULL;
		}
#ifdef KSOCKET_UNIX	
	}
#endif
	success = true;
	return NULL;
#endif
}
