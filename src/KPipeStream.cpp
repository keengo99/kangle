/*
 * KPipeStream.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include "log.h"
#include "KPipeStream.h"
#include "kforwin32.h"
#include "ksocket.h"
#include "utils.h"
#include "kmalloc.h"
using namespace std;
KPipeStream::KPipeStream() {
	fd[0] = fd[1] = INVALIDE_PIPE;
	fd2[0] = fd2[1] = INVALIDE_PIPE;
#ifndef _WIN32
	tmo = 0;
#endif
	errorCount = 0;
}
void KPipeStream::waitClose()
{
#ifdef _WIN32
	//{{ent
	WaitForSingleObject(process.getPid(),-1);
	//}}
#else
	char buf[32];
	for(;;){
		int len = read(buf,sizeof(buf)-1);
		if(len<=0){
			return;
		}
		buf[len] = '\0';
		printf("%s",buf);
	}
#endif
}
void KPipeStream::closeAllOtherFile() {
#ifndef _WIN32
	int max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd == -1) {
		//		debug("cann't get max_fd=%d\n", max_fd);
		max_fd = 2048;
	}
	int start_fd = 3;
	if (m_debug > 0) {
		/*
		 * 如果是调试，我们不关stdin,stdout,stderr,方便调试。
		 */
		start_fd = 3;
	}
	//	debug("max_fd=%d\n",max_fd);
	for (int i = start_fd; i < max_fd; i++) {
		if (i != fd[0] && i != fd[1]) {
			::close(i);
		}
	}
#endif
}
void KPipeStream::close() {
	if (fd[0] != INVALIDE_PIPE) {
		ClosePipe(fd[0]);
		fd[0] = INVALIDE_PIPE;
	}
	if (fd[1] != INVALIDE_PIPE) {
		ClosePipe(fd[1]);
		fd[1] = INVALIDE_PIPE;
	}
	if (fd2[0] != INVALIDE_PIPE) {
		ClosePipe(fd2[0]);
		fd2[0] = INVALIDE_PIPE;
	}
	if (fd2[1] != INVALIDE_PIPE) {
		ClosePipe(fd2[1]);
		fd2[1] = INVALIDE_PIPE;
	}
}
KPipeStream::~KPipeStream() {
	close();
}

void KPipeStream::setPipe(pid_t child_pid) {
	if (child_pid == 0) {
		//it is main
		ClosePipe(fd[1]);
		ClosePipe(fd2[0]);
		fd[1] = fd2[1];
	} else {
		ClosePipe(fd[0]);
		ClosePipe(fd2[1]);
		fd[0] = fd2[0];
	}
	fd2[0] = INVALIDE_PIPE;
	fd2[1] = INVALIDE_PIPE;
	process.bind(child_pid);
//	pid = child_pid;
}
bool KPipeStream::create() {
	if (!create(fd)) {
		return false;
	}
	return create(fd2);
}
void KPipeStream::setTimeOut(int tmo) {
	//{{ent
#ifdef _WIN32
	COMMTIMEOUTS tm;
	memset(&tm,0,sizeof(tm));
	tm.ReadIntervalTimeout = tmo*1000;
	SetCommTimeouts(fd[0],&tm);
	SetCommTimeouts(fd[1],&tm);
#else
	//}}
	this->tmo = tmo;
//{{ent
#endif
//}}
}
bool KPipeStream::create(PIPE_T *fd) {
	//{{ent
#ifdef _WIN32
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;
	if (!CreatePipe(&fd[0],&fd[1],&sa,0)) {
		return false;
	}
	return true;
#else
	//}}
	return pipe(fd) == 0;
//{{ent
#endif
//}}
}
int KPipeStream::read(char *buf, int len) {
//{{ent
#ifdef _WIN32
	unsigned long bytesRead;
	if(!ReadFile(fd[READ_PIPE],buf,len,&bytesRead,NULL)) {
		//debug("readPipe error errno=%d\n",GetLastError());
		return -1;
	}
	return bytesRead;

#else	
	//}}
	if (!wait_socket_event(fd[READ_PIPE], false, tmo)) {
		killChild();
	}
	return ::read(fd[READ_PIPE], buf, len);
//{{ent
#endif
//}}
}
int KPipeStream::write(const char *buf, int len) {
	//{{ent
#ifdef _WIN32
	unsigned long bytesRead;
	if(!WriteFile(fd[WRITE_PIPE],buf,len,&bytesRead,NULL)) {
		//debug("writePipe error errno=%d\n",GetLastError());
		return -1;
	}
	return bytesRead;
#else
	//}}
	if (!wait_socket_event(fd[WRITE_PIPE], true, tmo)) {
		killChild();
	}
	return ::write(fd[WRITE_PIPE], buf, len);
//{{ent
#endif
//}}
}
void KPipeStream::killChild() {
	process.kill();
}
bool KPipeStream::writeString(const char *str) {
	int len = 0;
	if (str) {
		len = (int)strlen(str);
	}
	if (!write_all((char *) &len, sizeof(len))) {
		return false;
	}
	if (str) {
		return write_all(str, len) == STREAM_WRITE_SUCCESS;
	}
	return true;
}
char *KPipeStream::readString(bool &result) {
	int len;
	if (!read_all((char *) &len, sizeof(len))) {
		result = false;
		return NULL;
	}
	if (len <= 0) {
		result = true;
		return NULL;
	}
	char *str = (char *) xmalloc(len+1);
	if (str == NULL) {
		result = false;
		return NULL;
	}
	str[len] = '\0';
	result = read_all(str, len);
	if (!result) {
		xfree(str);
		return NULL;
	}
	return str;
}
bool KPipeStream::writeInt(int value) {
	return write_all((char *) &value, sizeof(value)) == STREAM_WRITE_SUCCESS;
}
int KPipeStream::readInt(bool &result) {
	int value = 0;
	result = read_all((char *) &value, sizeof(value));
	return value;
}
bool KPipeStream::create_name(const char *read_pipe, const char *write_pipe) {
//{{ent
#ifdef _WIN32	
	assert(fd[0] == INVALIDE_PIPE && fd[1] == INVALIDE_PIPE);
	fd[READ_PIPE] = CreateNamedPipe(
			read_pipe, // pipe name
			PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED, // read/write access
			PIPE_TYPE_BYTE |PIPE_READMODE_BYTE| // message-read mode
			PIPE_WAIT, // blocking mode
			PIPE_UNLIMITED_INSTANCES, // max. instances
			1024, // output buffer size
			1024, // input buffer size
			0,//conf.time_out*1000, // client time-out
			NULL); // default security attribute
	fd[WRITE_PIPE] = CreateNamedPipe(
			write_pipe, // pipe name
			PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED, // read/write access
			PIPE_TYPE_BYTE | // message-read mode
			PIPE_WAIT, // blocking mode
			PIPE_UNLIMITED_INSTANCES, // max. instances
			1024, // output buffer size
			1024, // input buffer size
			0,//conf.time_out*1000, // client time-out
			NULL); // default security attribute
#endif
//}}
	return true;
}
bool KPipeStream::connect_name(const char *read_pipe, const char *write_pipe) {
//{{ent
#ifdef _WIN32
	assert(fd[0] == INVALIDE_PIPE && fd[1] == INVALIDE_PIPE);
	fd[READ_PIPE] = CreateFile(read_pipe,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
	fd[WRITE_PIPE] = CreateFile(write_pipe,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
#endif
//}}
	return (fd[WRITE_PIPE] != INVALIDE_PIPE);
}
