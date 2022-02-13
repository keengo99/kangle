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

#ifndef _WIN32
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils.h"
#include "log.h"
#include "KLogElement.h"
#include "KStringBuf.h"
#define OPEN_FILE
#include "kforwin32.h"
#include "kmalloc.h"
#include<string>
#include "KMutex.h"

#define KLOG_TO_SYSLOG		0
#define KLOG_TO_USER		1
#define KLOG_TO_PRINT		2


void my_vklog(int level, const char *fmt, va_list ap) {
	if (m_debug > 0) {
		if (level > m_debug) {
			return;
		}
		vprintf(fmt, ap);
		return;
	}
	if (level > conf.log_level) {
		return;
	}
	errorLogger.startLog();
	time_t ltime;
	time(&ltime);
	char tm[30];
	CTIME_R(&ltime, tm, sizeof(tm));
	tm[19] = 0;
	errorLogger.log("%s|", tm);
	errorLogger.vlog(fmt, ap);
	errorLogger.endLog(true);
}


void set_logger()
{
	if (conf.error_rotate_size > 0) {
		errorLogger.rotateSize = conf.error_rotate_size;
	} else {
		errorLogger.setRotateTime(conf.log_rotate);
		errorLogger.rotateSize = conf.log_rotate_size;
	}
	errorLogger.log_handle = false;
	accessLogger.setRotateTime(conf.log_rotate);
	accessLogger.rotateSize = conf.log_rotate_size;
	accessLogger.logs_day = conf.logs_day;
	accessLogger.logs_size = conf.logs_size;
	accessLogger.log_handle = conf.log_handle;
}
int klog_start() {
	if (m_debug) {
		accessLogger.place = LOG_PRINT;
		errorLogger.place = LOG_PRINT;
	} else {
		errorLogger.errorLog = true;
		errorLogger.place = LOG_FILE;
		std::string log_file;
#ifdef KANGLE_VAR_DIR
		log_file = KANGLE_VAR_DIR;
#else
		log_file = conf.path + "/var";
#endif
		log_file += "/server.log";
		errorLogger.setPath(log_file.c_str());
		if (!errorLogger.open()) {
			fprintf(stderr, "cann't open log file (server.log) for write\n");
			errorLogger.place = LOG_PRINT;
		}
		accessLogger.place = LOG_FILE;
		std::string logpath;
		if(conf.access_log[0]!='|' && !isAbsolutePath(conf.access_log)){
#ifdef KANGLE_VAR_DIR
			logpath = KANGLE_VAR_DIR;
			logpath += "/";
#else
			logpath = conf.path + "/var/";
#endif
		}
		logpath+=conf.access_log;
		accessLogger.setPath(logpath);
		if (!accessLogger.open()) {
			fprintf(stderr, "cann't open log file for write\n");
			accessLogger.place = LOG_PRINT;
		}
		set_logger();
	}
	klog_init(my_vklog);
	return 1;
}

