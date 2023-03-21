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
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "KLogElement.h"
#include "kforwin32.h"
#include "kmalloc.h"
#include "KLogHandle.h"
#include "utils.h"
#include "log.h"
#include "ksocket.h"
KLogElement accessLogger;
KLogElement errorLogger;
using namespace std;

KLogElement::KLogElement() {
	mkdir_flag = false;
	place = LOG_PRINT;
	rotate_size = 0;
	log_file_size = 0;
	//errorLog = false;
	logs_day = 0;
	logs_size = 0;
	log_handle = true;
}

KLogElement::~KLogElement() {
	close();
}
bool KLogElement::open() {
	assert(!fp.opened());
	if (path == "/nolog") {
		place = LOG_NONE;
		return true;
	}
	if (fp.opened()) {
		return false;
	}
	log_file_size = 0;
#ifdef ENABLE_PIPE_LOG
	if (path[0] == '|') {
		//pipe log
		std::vector<char*> args;
		char* cmd_buf = xstrdup(path.c_str() + 1);
		explode_cmd(cmd_buf, args);
		int args_count = args.size() + 1;
		char** arg = new char* [args_count];
		int i = 0;
		for (unsigned j = 0; j < args.size(); j++) {
			if (args[j] == NULL) {
				continue;
			}
			arg[i] = args[j];
			i++;
		}
		arg[i] = NULL;
		PIPE_T fd[2];
		bool result = false;
		if (KPipeStream::create(fd)) {
			pid_t pid;
			result = createProcess(NULL, arg, NULL, NULL, fd[READ_PIPE], INVALIDE_PIPE, INVALIDE_PIPE, pid);
			ClosePipe(fd[READ_PIPE]);
			if (result) {
				fp.setHandle(fd[WRITE_PIPE]);
#ifdef _WIN32
				CloseHandle(pid);
#endif
			}
		}
		delete[] arg;
		free(cmd_buf);
		return result;
	}
#endif
	struct _stati64 sbuf;
	if (lstat(path.c_str(), &sbuf) == 0) {
		log_file_size = sbuf.st_size;
	}
	int flag = 0;
#ifndef _WIN32
	if (uid > 0 || gid > 0) {
		flag = KFILE_NOFOLLOW;
	}
#endif
	fp.open(path.c_str(), fileAppend, flag);
	if (!fp.opened() && mkdir_flag) {
		fprintf(stderr, "cann't open log file=[%s] try to create the dir\n", path.c_str());
		char* str = strdup(path.c_str());
		char* p = str + strlen(str) - 1;
		while (p > str) {
			if (*p == '/' || *p == '\\') {
				*p = 0;
				break;
			}
			p--;
		}
		mkdir(str, 0755);
		xfree(str);
		fp.open(path.c_str(), fileAppend, flag);
		if (!fp.opened()) {
			mkdir_flag = false;
		}
	}
	return fp.opened();
}
void KLogElement::close() {
	fp.close();
}
bool KLogElement::checkRotate(time_t nowTime) {
	lock.Lock();
	if (!rotate.isOpen() && rotate.check(nowTime)) {
		rotateLog();
		lock.Unlock();
		return true;
	}
	lock.Unlock();
	return false;
}
void KLogElement::rotateLog() {
	struct tm tm;
	if (!fp.opened()) {
		return;
	}
#ifdef ENABLE_PIPE_LOG
	if (path[0] == '|') {
		//pipe
		return;
	}
#endif
	KStringBuf nf(256);
	char* t = xstrdup(path.c_str());
	if (t == NULL) {
		return;
	}
	char* p = strrchr(t, '.');
	if (p) {
		*p = '\0';
	}
	time_t now_tm;
	now_tm = time(NULL);
	localtime_r(&now_tm, &tm);
	nf << t;
	nf.add(tm.tm_mon + 1, "%02d");
	nf.add(tm.tm_mday, "%02d");
	nf << "_";
	nf.add(tm.tm_hour, "%02d");
	nf.add(tm.tm_min, "%02d");
	nf.add(tm.tm_sec, "%02d");
	if (p) {
		nf << "." << (p + 1);
	}
	close();
	int ret = rename(path.c_str(), nf.c_str());
	int err = GetLastError();
	open();
	xfree(t);
	char tms[30];
	CTIME_R(&now_tm, tms, sizeof(tms));
	tms[19] = 0;
	if (ret == 0) {
		errorLogger.log("%s|rotate log file [%s] to [%s] success\n", tms, path.c_str(), nf.c_str());
		logHandle.handle(this, nf.c_str());
	} else {
		errorLogger.log("%s|cann't rotate log file [%s] to [%s],error=[%d]\n", tms, path.c_str(), nf.c_str(), err);
	}
}

void KLogElement::setPath(KString path) {
	lock.Lock();
	if (path != "/nolog" && worker_index > 0) {
		KStringBuf s;
		s << path << worker_index;
		s.str().swap(this->path);
	} else {
		this->path.swap(path);
	}
	close();
	lock.Unlock();
}
