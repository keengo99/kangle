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
enum {
	LOG_NONE, LOG_PRINT, LOG_FILE
};
class KLogElement : public KCountable {
public:
	KLogElement();
	virtual ~KLogElement();
	inline KLogElement &operator <<(const char *str) {
		write(str);
		return *this;
	}
	KLogElement &operator <<(int value) {
		log("%d",value);
		return *this;
	}
	inline void write(const char *str,int len)
	{
		if (place == LOG_NONE) {
			return;
		}
	    if (place == LOG_PRINT) {
		fprintf(stderr, "%s", str);
		return;
	    }
		if (!opened) {
			open();
		}
		logSize += fp.write(str, len);
	}
	inline void write(const char *str) {
		write(str,(int)strlen(str));
	}

	inline void write(int value) {
		log("%d",value);
	}
	inline void vlog(const char *fmt, va_list ap) {
		if (place == LOG_NONE) {
			return;
		}
		if (place == LOG_PRINT) {
			vfprintf(stderr, fmt, ap);
			return;
		}
		if (!opened) {
			open();
		}
		logSize += fp.vfprintf(fmt, ap);
	}
	inline void log(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		vlog(fmt, ap);
		va_end(ap);
	}
	inline void startLog() {
		lock.Lock();
	}
	inline void endLog(bool flush = false) {
		if(flush){
			fp.flush();
		}
		if (rotateSize > 0 && logSize >= rotateSize) {
			rotateLog();
		}
		lock.Unlock();
	}
	bool open();
	inline void checkSizeRotate()
	{
		if (rotateSize > 0 && logSize >= rotateSize) {
			rotateLog();
		}
	}
	bool checkRotate(time_t nowTime);
	void buildXML(std::stringstream &s);
	void setRotateTime(const char *str)
	{
		lock.Lock();
		rotate_time = str;
		rotate.set(str);
		lock.Unlock();
	}
	void getRotateTime(std::string &str)
	{
		lock.Lock();
		str = rotate_time;
		lock.Unlock();
	}
	void setPath(std::string path);
	friend class KVirtualHost;
	friend class KVirtualHostManage;
	std::string path;
	INT64 rotateSize;
	int place;
	bool mkdirFlag;
	unsigned logs_day;
	INT64 logs_size;
	void close();
	// «∑Ò «errorLog;
	bool log_handle;
	bool errorLog;
private:
	void rotateLog();
	std::string rotate_time;
	//	bool noLog;
	bool opened;
	INT64 logSize;
	KFile fp;
	KTimeMatch rotate;
	unsigned max;
	unsigned uid;
	unsigned gid;
	KMutex lock;
};
//system default access loger
extern KLogElement accessLogger;
extern KLogElement errorLogger;
#endif /*KLOGELEMENT_H_*/
