/*
 * KSingleProgram.cpp
 *
 *  Created on: 2010-4-30
 *      Author: keengo
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#endif
#include <stdio.h>

#include "KSingleProgram.h"
#include "log.h"
#include "kmalloc.h"
#include "kforwin32.h"
KSingleProgram singleProgram;
KSingleProgram::KSingleProgram() {
	kfinit(fd);
#ifdef _WIN32
	memset(&ov,0,sizeof(ov));
	ov.Offset = 100;
#endif
}
KSingleProgram::~KSingleProgram() {
	if (kflike(fd)) {
		kfclose(fd);
	}
}
bool KSingleProgram::deletePid() {
	if (!savePid(0)) {
		return false;
	}
	this->unlock();
	return true;
}
bool KSingleProgram::savePid(int savePid) {
	if (!kflike(fd)) {
		return false;
	}
	char buf[16];
	memset(buf, 0, sizeof(buf));
#ifndef _WIN32
	lseek(fd, 0, SEEK_SET);
#else
	LARGE_INTEGER li;
	li.QuadPart = 0;
	SetFilePointer(fd,li.LowPart,&li.HighPart,FILE_BEGIN);
#endif
	snprintf(buf, sizeof(buf) - 2, "%d", savePid);
	kfwrite(fd, buf, sizeof(buf));
	return true;
}
void KSingleProgram::unlock() {
	if (kflike(fd)) {
		kfclose(fd);
		kfinit(fd);
	}
}
bool KSingleProgram::lock(const char *pidFile) {
#ifndef _WIN32
	fd = open(pidFile, O_RDWR | O_CREAT | O_TRUNC, 0644);
#else
	SECURITY_ATTRIBUTES sa;
	memset(&sa,0,sizeof(sa));
	sa.bInheritHandle = FALSE;
	fd = CreateFile(pidFile,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,CREATE_ALWAYS,0,NULL);
#endif
	if (!kflike(fd)) {
		klog(KLOG_ERR, "cann't open pid file[%s] fd=[%d]\n", pidFile,fd);
		return false;
	}
#ifndef _WIN32
	struct flock lock;
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	if (fcntl(fd, F_SETLKW, &lock) == -1) {
		klog(KLOG_ERR, "lock failed errno=%d.\n", errno);
		return false;
	}
#else
	if (!LockFileEx(fd,0,0,1,0,&ov)) {
		klog(KLOG_ERR, "lock failed errno=%d.\n", errno);
		return false;
	}
#endif
	return savePid(getpid());
}
int KSingleProgram::checkRunning(const char *pidFile) {
#ifndef _WIN32
	fd = open(pidFile, O_RDWR);
#else
	SECURITY_ATTRIBUTES sa;
	memset(&sa,0,sizeof(sa));
	sa.bInheritHandle = FALSE;
	fd = CreateFile(pidFile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
#endif
	if (!kflike(fd)) {
		return 0;
	}
#ifndef _WIN32
	struct flock lock;
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	if (fcntl(fd, F_SETLK, &lock) != -1) {
		close(fd);
		fd = 0;
		return 0;
	}
#else
	if (LockFileEx(fd,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY ,0,1,0,&ov)) {
		kfclose(fd);
		kfinit(fd);
		return 0;
	}
#endif
	char buf[16];
	memset(buf, 0, sizeof(buf));
	kfread(fd, buf, sizeof(buf) - 1);
	kfclose(fd);
	kfinit(fd);
	return atoi(buf);
}
