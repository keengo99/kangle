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
#ifndef KLOGELEMENT_H_
#define KLOGELEMENT_H_
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "global.h"
#include "KCountable.h"
#include "KTimeMatch.h"
#include "KMutex.h"
#include "KFileName.h"
#include "KStringBuf.h"
#define LOG_NONE  0
#define LOG_PRINT 1
#define LOG_FILE  2

class KLogElement : public KAtomCountable
{
public:
	KLogElement();
	virtual ~KLogElement();
	inline KLogElement& operator <<(const char* str) {
		write(str);
		return *this;
	}
	KLogElement& operator <<(int value) {
		log("%d", value);
		return *this;
	}
	inline void write(const char* str, int len) {
		if (place == LOG_NONE) {
			return;
		}
		if (place == LOG_PRINT) {
			fprintf(stderr, "%s", str);
			return;
		}
		if (!fp.opened()) {
			open();
		}
		log_file_size += fp.write(str, len);
	}
	inline void write(const char* str) {
		write(str, (int)strlen(str));
	}

	inline void write(int value) {
		log("%d", value);
	}
	inline void vlog(const char* fmt, va_list ap) {
		if (place == LOG_NONE) {
			return;
		}
		if (place == LOG_PRINT) {
			vfprintf(stderr, fmt, ap);
			return;
		}
		if (!fp.opened()) {
			open();
		}
		log_file_size += fp.vfprintf(fmt, ap);
	}
	inline void log(const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		vlog(fmt, ap);
		va_end(ap);
	}
	inline void startLog() {
		lock.Lock();
	}
	inline void endLog(bool flush = false) {
		if (flush) {
			fp.flush();
		}
		if (rotate_size > 0 && log_file_size >= rotate_size) {
			rotateLog();
		}
		lock.Unlock();
	}
	bool open();
	inline void checkSizeRotate() {
		if (rotate_size > 0 && log_file_size >= rotate_size) {
			rotateLog();
		}
	}
	bool checkRotate(time_t nowTime);
	void setRotateTime(const char* str) {
		lock.Lock();
		rotate_time = str;
		rotate.set(str);
		lock.Unlock();
	}
	void getRotateTime(KString& str) {
		lock.Lock();
		str = rotate_time;
		lock.Unlock();
	}
	void setPath(KString path);
	void close();
	friend class KVirtualHost;
	friend class KVirtualHostManage;
	KString path;
	INT64 rotate_size;
	INT64 log_file_size;
	INT64 logs_size;
	unsigned logs_day;
#ifndef _WIN32
	//unsigned max;
	unsigned uid;
	unsigned gid;
#endif
	u_char place;
	bool mkdir_flag;
	bool log_handle;
private:
	void rotateLog();
	KString rotate_time;
	KFile fp;
	KTimeMatch rotate;
	KMutex lock;
};
//system default access loger
extern KLogElement accessLogger;
extern KLogElement errorLogger;
#endif
