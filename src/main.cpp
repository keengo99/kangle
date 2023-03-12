/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "global.h"
#ifndef _WIN32
#include <pthread.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "kforwin32.h"
#else
#include <direct.h>
#include <stdlib.h>
#include "kforwin32.h"
#include <Mswsock.h>
#include <Objbase.h>
#define _WIN32_SERVICE
#endif
#include<iostream>
#include <time.h>
#include <string>
#include <stdio.h>
#ifdef ENABLE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif
#include "utils.h" 
#include "do_config.h"
#include "log.h"
#include "kserver.h"
#include "extern.h"
 //#define OPEN_FILE
#include "http.h"
#include "cache.h"
#include "KListenConfigParser.h"
#include "kthread.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "KXml.h"
#include "KVirtualHostManage.h"
#include "KHttpServerParser.h"
#include "KFastcgiFetchObject.h"
#include "KDsoExtendManage.h"
#include "KSingleProgram.h"
#include "KServerListen.h"
#include "KProcessManage.h"
#include "KGzip.h"
#include "KLogElement.h"
#include "api_child.h"
#include "time_utils.h"
#include "directory.h"
#include "KHttpDigestAuth.h"
#include "KObjectList.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KCdnContainer.h"
#include "KWriteBackManager.h"
#include "KDynamicListen.h"
#include "kaddr.h"
#include "kmalloc.h"
#include "KLogDrill.h"
#include "ssl_utils.h"
#include "KSSLSniContext.h"
#include "WhmPackageManage.h"
#include "kfiber.h"
#include "KHttpServer.h"
#include "HttpFiber.h"
#include "KSimulateRequest.h"

//void flush_net_request(time_t now_time);


#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif
char* lang = NULL;
int m_debug = 0;
bool skipCheckRunning = false;
int program_rand_value;
using namespace std;
int m_pid = 0;
int m_ppid = 0;
extern int my_uid;
int child_pid = 0;
int reboot_flag = 0;
int serial = 0;
int worker_index = 0;
bool nodaemon = false;
bool nofork = false;
int open_file_limit = 0;
bool test();
int GetNumberOfProcessors();
#ifdef _WIN32
HANDLE active_event = INVALID_HANDLE_VALUE;
HANDLE shutdown_event = INVALID_HANDLE_VALUE;
HANDLE signal_pipe = INVALID_HANDLE_VALUE;
static HANDLE notice_event = INVALID_HANDLE_VALUE;
void start_safe_service();
std::vector<WorkerProcess*> workerProcess;
#else
std::map<int, WorkerProcess*> workerProcess;
#endif
/*
 the main process and child process communicate pipe
 */

void my_exit(int code) {
#ifdef _WIN32
	SetUnhandledExceptionFilter(NULL);
#endif
	conf.gam->unloadAllApi();
#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(), code);
#endif
	exit(code);
}
#ifndef _WIN32
void killworker(int sig) {
	std::map<int, WorkerProcess*>::iterator it;
	for (it = workerProcess.begin(); it != workerProcess.end(); it++) {
		kill((*it).first, sig);
	}
	/*
	if (sig==SIGHUP) {
		for(it=workerProcess.begin();it!=workerProcess.end();it++){
			delete (*it).second;
		}
		workerProcess.clear();
	}
	*/
}
#endif
//}}
#ifdef MALLOCDEBUG
extern "C" {
	extern void __libc_freeres();
}
#ifdef _WIN32
void LogEvent(LPCTSTR pFormat, ...);
#endif
int clean_memory_leak_fiber(void* arg, int argc) {

	printf("free all know memory\n");
	selector_manager_close();
#ifndef HTTP_PROXY
	conf.gam->shutdown();
#endif
#ifdef ENABLE_VH_FLOW
	conf.gvm->dumpFlow();
#endif
#ifdef ENABLE_LOG_DRILL
	flush_log_drill();
#endif
	clean_config();
	delete conf.sysHost;
	conf.sysHost = NULL;
	packageManage.clean();
	shutdown_http_server();
#ifndef HTTP_PROXY
	delete conf.gvm;
	delete conf.gam;
#endif
	if (conf.dem) {
		delete conf.dem;
	}
#ifdef ENABLE_WRITE_BACK
	writeBackManager.destroy();
#endif
	cache.freeAllObject();
#ifdef ENABLE_DIGEST_AUTH
	KHttpDigestAuth::flushSession(kgl_current_sec + 172800);
#endif
	server_container->Clean();
	klang.clear();
	if (conf.dnsWorker) {
		kasync_worker_release(conf.dnsWorker);
		conf.dnsWorker = NULL;
	}
	if (conf.ioWorker) {
		kasync_worker_release(conf.ioWorker);
		conf.ioWorker = NULL;
	}
	printf("wait for all thread close\n");
	int work_count, free_count;
	for (;;) {
		kthread_get_count(&work_count, &free_count);
		if (work_count == 0) {
			break;
		}
		my_msleep(100);
	}
	kthread_close_all_free();
#ifdef KSOCKET_SSL
	kssl_clean();
#endif
	return 0;
}
#endif
void checkMemoryLeak() {
#ifdef MALLOCDEBUG
	if (!conf.mallocdebug) {
		fprintf(stderr, "mallocdebug is not active\n");
		return;
	}
	my_msleep(1000);
	kasync_main(clean_memory_leak_fiber, NULL, 0);
	my_msleep(1500);
	int leak_count = dump_memory_leak(0, -1);
	printf("total memory leak count=[%d]\n", leak_count);
#ifndef _WIN32
	__libc_freeres();
#endif
	exit(0);
#endif
}
int shutdown_fiber(void* arg, int got) {
	quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	KAccess::ShutdownMarkModule();
	cache.shutdown_disk(true);

	//conf.gvm->UnBindGlobalListens(conf.service);
	conf.gvm->shutdown();
	conf.default_cache = 0;
#ifdef ENABLE_DISK_CACHE
	if (conf.disk_cache > 0) {
		saveCacheIndex();
		if (dci) {
			delete dci;
			dci = nullptr;
		}
	}
#endif
#ifdef ENABLE_VH_RUN_AS
	conf.gam->shutdown();
#endif
#ifdef ENABLE_VH_FLOW
	conf.gvm->dumpFlow();
#endif
	if (conf.dem) {
		conf.dem->shutdown();
	}
	accessLogger.close();
	kconfig::shutdown();
	klog(KLOG_ERR, "shutdown\n");
	errorLogger.close();
	singleProgram.deletePid();
	quit_program_flag = PROGRAM_QUIT_SHUTDOWN;
#ifndef MALLOCDEBUG
	/**
	 *  malloc debug model. do not exit right now.
	 *  program will free all know memory and log leak memory.
	 * */
	exit(0);
#endif
	return 0;
}
void shutdown_safe_process() {
	exit(0);
}
void create_shutdown_fiber() {
	/**
	* we sure call shutdown function use fiber.
	*/
	quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	kselector* selector = get_selector_by_index(0);
	kfiber_create2(selector, shutdown_fiber, NULL, 0, 0, NULL);
}
void shutdown_signal(int sig) {
#ifndef _WIN32
	/**
	* on unix from signal do not call any function. only can set flag.
	* this cause main selector call create_shutdown_fiber.
	*/
	if (workerProcess.empty()) {
		if (quit_program_flag > 0) {
			return;
		}
		quit_program_flag = PROGRAM_QUIT_PREPARE;
		return;
	}
	killworker(sig);
	if (sig != SIGHUP) {
		quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	}
	return;
#endif
	create_shutdown_fiber();
}

#ifdef _WIN32
kgl_auto_cstr get_signal_pipe_name(int pid) {
	KStringStream spipe;
	spipe << "\\\\.\\pipe\\kangle_signal_" << pid;
	return spipe.steal();
}
int kill(int pid, char sig) {
	auto spipe = get_signal_pipe_name(pid);
	HANDLE fd = CreateFile(spipe.get(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (!kflike(fd)) {
		return -1;
	}
	DWORD bytesWrite;
	BOOL result = WriteFile(fd, &sig, 1, &bytesWrite, NULL);
	kfclose(fd);
	if (result) {
		return 0;
	}
	return -1;
}
KTHREAD_FUNCTION wait_shutdown(void* arg) {
	WaitForSingleObject(shutdown_event, INFINITE);
	shutdown_signal(0);
	KTHREAD_RETURN;
}
bool signal_recved_pipe() {
	char buf[4];
	DWORD bytesRead;
	BOOL result = ReadFile(signal_pipe, buf, 1, &bytesRead, NULL);
	if (!result) {
		klog(KLOG_ERR, "readPipe error errno=%d\n", GetLastError());
		my_msleep(500);
		return true;
	}
	klog(KLOG_ERR, "recv signal [%c]\n", buf[0]);
	switch (buf[0]) {
	case 'r':
		do_config(false);
		break;
	case 'v':
		do_config(false);
		break;
	case 'q':
		shutdown_signal(0);
		return false;
	default:
		break;
	}
	return true;
}
void create_signal_pipe() {
	auto spipe = get_signal_pipe_name((int)getpid());
	signal_pipe = CreateNamedPipe(
		spipe.get(), // pipe name
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // read/write access
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | // message-read mode
		PIPE_WAIT, // blocking mode
		PIPE_UNLIMITED_INSTANCES, // max. instances
		1024, // output buffer size
		1024, // input buffer size
		0,//conf.time_out*1000, // client time-out
		NULL);
}
KTHREAD_FUNCTION signal_thread(void* arg) {
	if (!kflike(signal_pipe)) {
		wait_shutdown(NULL);
		KTHREAD_RETURN;
	}
	HANDLE ev[3];
	int max_ev = 1;
	if (kflike(shutdown_event)) {
		max_ev = 2;
		ev[1] = shutdown_event;
	}
	for (;;) {
		OVERLAPPED ol;
		memset(&ol, 0, sizeof(ol));
		ol.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ConnectNamedPipe(signal_pipe, &ol)) {
			if (!signal_recved_pipe()) {
				CloseHandle(ol.hEvent);
				CloseHandle(signal_pipe);
				break;
			}
		} else {
			int error = GetLastError();
			if (error == ERROR_PIPE_CONNECTED) {
				if (!signal_recved_pipe()) {
					CloseHandle(ol.hEvent);
					CloseHandle(signal_pipe);
					break;
				}
			} else if (error == ERROR_IO_PENDING) {
				ev[0] = ol.hEvent;
				//WAIT_OBJECT_0 == WaitForSingleObject(ol.hEvent,INFINITE)){
				//bResult = TRUE;
				int ret = WaitForMultipleObjects(max_ev, ev, FALSE, INFINITE);
				int index = ret - WAIT_OBJECT_0;
				if (index == 0) {
					if (!signal_recved_pipe()) {
						CloseHandle(ol.hEvent);
						CloseHandle(signal_pipe);
						break;
					}
				}
				if (index == 1) {
					shutdown_signal(0);
				}
			} else {
				my_msleep(500);
			}
		}
		CloseHandle(ol.hEvent);
		CloseHandle(signal_pipe);
		create_signal_pipe();
	}
	KTHREAD_RETURN;
}
#endif

#ifdef ENABLE_DISK_CACHE
bool create_dir(const char* dir) {
	mkdir(dir, 448);
	return true;
}
void create_cache_dir(const char* disk_dir) {
	KString path;
	if (disk_dir && *disk_dir) {
		path = disk_dir;
		pathEnd(path);
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	KStringBuf s;
	create_dir(path.c_str());
	for (int i = 0; i <= CACHE_DIR_MASK1; i++) {
		s << path.c_str();
		s.add_as_hex(i);
		if (!create_dir(s.c_str())) {
			return;
		}
		s.clear();
		for (int j = 0; j <= CACHE_DIR_MASK2; j++) {
			s << path.c_str();
			s.add_as_hex(i);
			s << PATH_SPLIT_CHAR;
			s.add_as_hex(j);
			if (!create_dir(s.c_str())) {
				return;
			}
			s.clear();
		}
	}
#if 0
	KStringBuf index_name;
	index_name << path << "index";
	FILE* fp = fopen(index_name.getString(), "wb");
	if (fp == NULL) {
		fprintf(stderr, "cann't open cache index file for write[%s]\n", index_name.getString());
		return;
	}
	HttpObjectIndexHeader indexHeader;
	memset(&indexHeader, 0, sizeof(HttpObjectIndexHeader));
	indexHeader.head_size = sizeof(HttpObjectIndexHeader);
	indexHeader.block_size = sizeof(HttpObjectIndex);
	indexHeader.state = INDEX_STATE_CLEAN;
	indexHeader.cache_dir_mask1 = CACHE_DIR_MASK1;
	indexHeader.cache_dir_mask2 = CACHE_DIR_MASK2;
	fwrite((char*)&indexHeader, 1, sizeof(indexHeader), fp);
	fclose(fp);
#endif
	fprintf(stderr, "create cache dir success\n");
}
#endif
void console_call_reboot() {
	//quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	shutdown_signal(10);
	//{{ent
#ifdef _WIN32	
	if (shutdown_event == NULL) {
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		GetStartupInfo(&si);
		BOOL bResult = CreateProcess(
			conf.program.c_str(), // file to execute
			NULL, // command line
			NULL, // pointer to process SECURITY_ATTRIBUTES
			NULL, // pointer to thread SECURITY_ATTRIBUTES
			FALSE, // handles are not inheritable
			NORMAL_PRIORITY_CLASS | DETACHED_PROCESS | CREATE_NO_WINDOW, // creation flags
			NULL, // pointer to new environment block
			NULL, // name of current directory
			&si, // pointer to STARTUPINFO structure
			&pi // receives information about new process
		);
		if (bResult) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
	} else {
		if (notice_event) {
			SetEvent(notice_event);
		}
	}
#endif
	//}}
}
//{{ent
#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType) {
	debug("catch event = %d\n", fdwCtrlType);
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal.
	case CTRL_C_EVENT:
		shutdown_signal(0);
		return(TRUE);
		// CTRL-CLOSE: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT:
		shutdown_signal(0);
		return TRUE;
		// Pass other signals to the next handler.
	case CTRL_BREAK_EVENT:
		shutdown_signal(0);
		return TRUE;
	case CTRL_LOGOFF_EVENT:
		shutdown_signal(0);
		return FALSE;
	case CTRL_SHUTDOWN_EVENT:
		shutdown_signal(0);
		return FALSE;
	default:
		return FALSE;
	}

}
#endif
//}}
void sigcatch(int sig) {
#ifdef HAVE_SYSLOG_H
	klog(KLOG_INFO, "catch signal %d,my_pid=%d\n", sig, getpid());
#endif
#ifndef _WIN32
	//int status = 0;
	//int ret;
	signal(sig, sigcatch);
	switch (sig) {
	case SIGTERM:
	case SIGINT:
	case SIGQUIT:
		shutdown_signal(sig);
		break;
	case SIGHUP:
		if (workerProcess.size() > 0) {
			killworker(sig);
		} else {
			configReload = true;
		}
		break;
	case SIGUSR2:
		if (workerProcess.size() > 0) {
			killworker(SIGUSR2);
		} else {
#ifdef ENABLE_VH_FLOW
			flushFlowFlag = true;
#endif
#ifdef MALLOCDEBUG
			dump_memory_object = true;
#endif
		}
		break;
	default:
		return;
	}
#endif
}
void set_user() {
#if	!defined(_WIN32)
	if (conf.run_user.size() > 0) {
		int uid, gid;
		if (getuid() != 0) {
			fprintf(stderr, "I am not root user,cann't run as user[%s]\n", conf.run_user.c_str());
			return;
		}
		bool result = name2uid(conf.run_user.c_str(), uid, gid);
		if (!result) {
			klog(KLOG_ERR, "cann't find run_as user [%s]\n", conf.run_user.c_str());
		}
		if (result && conf.run_group.size() > 0) {
			result = name2gid(conf.run_group.c_str(), gid);
			if (!result) {
				klog(KLOG_ERR, "cann't find run_as group [%s]\n", conf.run_group.c_str());
			}
		}
		if (result) {
			chown(conf.tmppath.c_str(), uid, gid);
			setgid(gid);
			setuid(uid);
		}

	}

#endif	/* !_WIN32 */
}
void list_service() {
	return;
}
int service_to_signal(int sig, bool showError = true) {
	if (m_pid == 0) {
		if (showError) {
			fprintf(stderr, "Error,program is not running.\n");
		}
		return 0;
	}
	if (kill(m_pid, sig) == 0) {
		return m_pid;
	}
	if (showError) {
		fprintf(stderr, "Error ,while kill signal to pid=%d.\n", m_pid);
	}
	return 0;
}

bool create_file_path(char* argv0) {
	if (!get_path(argv0, conf.path)) {
		return false;
	}
	KFileName file;
	file.tripDir(conf.path);
#ifndef _WIN32
	conf.path = "/" + conf.path;
#endif
	conf.program = conf.path + PATH_SPLIT_CHAR + conf.program;
	conf.extworker = conf.path + PATH_SPLIT_CHAR + "extworker";
#ifdef _WIN32
	conf.extworker += ".exe";
#endif
	int p = (int)conf.path.find_last_of(PATH_SPLIT_CHAR);
	if (p > 0) {
		conf.path = conf.path.substr(0, p + 1);
	}
#ifdef KANGLE_TMP_DIR
	conf.tmppath = KANGLE_TMP_DIR;
	conf.tmppath += PATH_SPLIT_CHAR;
#else
	conf.tmppath = conf.path + PATH_SPLIT_CHAR + "tmp" + PATH_SPLIT_CHAR;
#endif
	mkdir(conf.tmppath.c_str(), 448);
	return true;
}
void shutdown_process(int pid, int sig) {
}
int clean_process_handle(const char* file, void* param) {
	int kangle_pid = *((int*)(param));
	if (filencmp(file, "kp_", 3) != 0) {
		return 0;
	}
	int fpid = atoi(file + 3);
	if (kangle_pid > 0 && fpid != kangle_pid) {
		return 0;
	}
	int pid = 0;
	int sig = 0;
	const char* p = strchr(file + 3, '_');
	if (p) {
		pid = atoi(p + 1);
		p = strchr(p + 1, '_');
		if (p) {
			sig = atoi(p + 1);
		}
#ifdef _WIN32
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if (hProcess != NULL) {
			TerminateProcess(hProcess, sig);
			CloseHandle(hProcess);
		}
#else
		kill(pid, sig);
#endif
	}
	KStringBuf s;
	s << conf.tmppath << file;
	char unix_file[512];
	FILE* fp = fopen(s.str().c_str(), "rb");
	if (fp) {
		int len = (int)fread(unix_file, 1, sizeof(unix_file) - 1, fp);
		if (len > 0) {
			unix_file[len] = '\0';
			unlink(unix_file);
		}
		fclose(fp);
	}
	unlink(s.str().c_str());
	return 0;
}
void clean_process(int pid) {
	list_dir(conf.tmppath.c_str(), clean_process_handle, (void*)&pid);
}

static int Usage(bool only_version = false) {
	printf(PROGRAM_NAME "/" VERSION "(%s) build with support:"
#ifdef KSOCKET_IPV6
		" ipv6"
#endif
#ifdef KSOCKET_SSL
		" ssl["
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		"S"
#endif
#ifdef TLSEXT_TYPE_next_proto_neg
		"N"
#endif
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
		"A"
#endif
#ifdef ENABLE_KSSL_BIO
		"B"
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
		"E"
#endif
		"] "
#endif
#ifdef ENABLE_HTTP2
		" h2"
#endif
#ifdef ENABLE_HTTP3
		" h3"
#endif
#ifdef ENABLE_PROXY_PROTOCOL
		" proxy"
#endif
#ifdef ENABLE_BIG_OBJECT
#ifdef ENABLE_BIG_OBJECT_206
		" big-object-206"
#else
		" big-object"
#endif
#endif
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
		" tproxy"
#endif
#endif
#ifdef ENABLE_DISK_CACHE
		" disk-cache"
#endif
#ifndef NDEBUG
		" debug"
#endif
#ifdef ENABLE_BROTLI
		" brotli"
#endif
#ifdef MALLOCDEBUG
		" malloc-debug"
#endif
		"\n", getServerType());
	printf("pcre version: %s\n", pcre_version());
#ifdef KSOCKET_SSL
	printf("openssl version: %s\n", SSLeay_version(SSLEAY_VERSION));
#endif
#ifdef UPDATE_CODE
	printf("UPDATE_CODE: %s\n", UPDATE_CODE);
#endif
	if (!only_version) {
		printf("Usage: " PROGRAM_NAME " [-hlqnra:] [-d level]\n"
			"   (no param to start server.)\n"
			"   [-h --help]      print the current message\n"
			"   [-d level]       start in debug model,level=0-3\n"
			"   [-r --reload]    reload config file graceful\n"
#ifndef _WIN32
			"   [--reboot]       reboot server\n"
#endif
#ifdef ENABLE_DISK_CACHE
			"   [-z [disk_dir]]  create disk cache directory\n"
#endif
			"   [-v --version]   show program version\n"
#ifndef _WIN32
			"   [-q]             shutdown\n"
			"   [-n]             start program not in daemon\n"
#endif
			"Report bugs to <keengo99@gmail.com>.\n"
			"");
	}
#ifdef ENABLE_JEMALLOC
	const char* j;
	size_t s = sizeof(j);
	mallctl("version", &j, &s, NULL, 0);
	printf("jemalloc version: [%s]\n", j);
#endif
	//checkMemoryLeak();
	fflush(stdout);
	my_exit(0);
	return 0;
}
bool create_path(char** argv) {
	char* argv0 = NULL;
#ifdef _WIN32
	char szFilename[512];
	::GetModuleFileName(NULL, szFilename, sizeof(szFilename) - 1);
	argv0 = xstrdup(szFilename);
	conf.diskName = szFilename;
	conf.diskName = conf.diskName.substr(0, 2);
#else
	argv0 = xstrdup(argv[0]);
#endif
	if (!create_file_path(argv0)) {
		xfree(argv0);
		return false;
	}
	xfree(argv0);
	return true;
}
int parse_args(int argc, char** argv) {
	extern char* optarg;
	int ret = 0;
#ifdef _WIN32
	char tmp[512];
#endif
	conf.log_level = -1;
	KString pidFile;
#ifdef KANGLE_VAR_DIR
	pidFile = KANGLE_VAR_DIR;
#else
	pidFile = conf.path;
	pidFile += "/var";
#endif
	mkdir(pidFile.c_str(), 0700);
	pidFile += PID_FILE;
	m_pid = singleProgram.checkRunning(pidFile.c_str());
	/*
	if (singleProgram.checkRunning(pidFile.c_str())) {
		m_pid = singleProgram.pid;
		if (m_pid <= 0) {
			fprintf(
					stderr,
					"Something error,have another program is running,but the pid=%d is not right.\n",
					m_pid);
			my_exit(1);
		}
	}
	*/
	if (argc > 1) {
		ret = 1;
	}
#ifndef _WIN32
	int c;
	struct option long_options[] = { { "reload", 0, 0, 'r' },
	{ "version", 0, 0, 'v' },
	{ "help", 0, 0,	'h' },
	{ "reboot", 0, 0, 'b' },
	{ 0, 0, 0, 0 } };
	int opt_index = 0;
	while ((c = getopt_long(argc, argv, "lgnrz:mfqa:d:hvr?", long_options,
		&opt_index)) != -1) {
		switch (c) {
		case 0:
			break;
		case 'b':
			ret = 0;
			service_to_signal(SIGINT, false);
			skipCheckRunning = true;
			break;

		case 'q':
			m_pid = service_to_signal(SIGTERM);
			if (m_pid > 0) {
				for (int i = 0; i < 200; i++) {
					if (kill(m_pid, 0) != 0) {
						break;
					}
					my_msleep(200);
				}
				printf("shutdown success.\n");
				my_exit(0);
			}
			printf("shutdown error.\n");
			my_exit(1);
			break;
#ifdef ENABLE_VH_FLOW
		case 'f':
			service_to_signal(SIGUSR2);
			my_exit(0);
			break;
#endif
		case 'r':
			service_to_signal(SIGHUP);
			my_exit(0);
			break;
#ifdef MALLOCDEBUG
		case 'm':
			service_to_signal(SIGUSR2);
			my_exit(0);
			break;
#endif
		case 'n':
			ret = 0;
			nodaemon = true;
			break;
		case 'g':
			nofork = true;
			break;
		case 'd':
			ret = 0;
			m_debug = atoi(optarg);
			nodaemon = true;
			printf("run as debug model(level=%d).\n", m_debug);
			break;
		case 'v':
			Usage(true);
			my_exit(0);
#ifdef ENABLE_DISK_CACHE
		case 'z':
			create_cache_dir(optarg);
			my_exit(0);
#endif
		case 'h':
		case '?':
			Usage();
			my_exit(0);
		default:
			Usage();
			my_exit(0);
		}
	}
#else
	//{{ent
	for (int i = 1; i < argc; i++) {
		if (get_param(argc, argv, i, "-r", tmp)) {
			service_to_signal('r');
			my_exit(0);
			break;
		}
		if (get_param(argc, argv, i, "-q", tmp)) {
			service_to_signal('q');
			my_exit(0);
			break;
		}
		if (get_param(argc, argv, i, "-l", tmp)) {
			service_to_signal('l');
			my_exit(0);
			break;
		}
		if (get_param(argc, argv, i, "--revh", tmp)) {
			service_to_signal('v');
			my_exit(0);
			break;
		}
#ifdef ENABLE_DISK_CACHE
		if (get_param(argc, argv, i, "-z", tmp)) {
			create_cache_dir(tmp);
			my_exit(0);
		}
#endif
		if (get_param(argc, argv, i, "-d", tmp)) {
			ret = 0;
			m_debug = atoi(tmp);
			continue;
		}
		if (get_param(argc, argv, i, "-n", NULL) || get_param(argc, argv, i, "-g", NULL)) {
			ret = 0;
			continue;
		}
		if (get_param(argc, argv, i, "-h", NULL)) {
			return Usage();
		}
		if (get_param(argc, argv, i, "--worker_index", tmp)) {
			worker_index = atoi(tmp);
			continue;
		}
		if (get_param(argc, argv, i, "--ppid", tmp)) {
			m_ppid = atoi(tmp);
			continue;
		}
		if (get_param(argc, argv, i, "--shutdown", tmp)) {
			ret = 0;
			shutdown_event = (HANDLE)string2int(tmp);
			continue;
		}
		if (get_param(argc, argv, i, "--active", tmp)) {
			ret = 0;
			active_event = (HANDLE)string2int(tmp);
			continue;
		}
		if (get_param(argc, argv, i, "--notice", tmp)) {
			ret = 0;
			notice_event = (HANDLE)string2int(tmp);
			continue;
		}
	}
	//}}
#endif
	if ((ret == 0) && (m_pid != 0) && !skipCheckRunning) {
		fprintf(stderr, "Start error,another program (pid=%d) is running.\n",
			m_pid);
		fprintf(stderr, "Try (%s -q) to close it.\n", argv[0]);
		my_exit(1);
	}
	return ret;
}
void init_signal() {
#ifndef _WIN32
	umask(0022);
	signal(SIGPIPE, SIG_IGN);

	signal(SIGHUP, sigcatch);

	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);

	signal(SIGUSR1, sigcatch);

	signal(SIGUSR2, sigcatch);

	signal(SIGQUIT, sigcatch);
	signal(SIGCHLD, SIG_DFL);
#endif
}

void init_safe_process() {
	KXml::fopen = (kxml_fopen)kfopen;
	KXml::fclose = (kxml_fclose)kfclose;
	KXml::fsize = (kxml_fsize)kfsize;
	KXml::fread = (kxml_fread)kfread;
}
void init_stderr() {
#ifdef ENABLE_TCMALLOC
	close(2);
	KString stderr_file = conf.path + "/var/stderr.log";
	KFile fp;
	if (fp.open(stderr_file.c_str(), fileAppend)) {
		FILE_HANDLE fd = fp.stealHandle();
		dup2(fd, 2);
		fprintf(stderr, "stderr is open success\n");
	}
#endif
}
void init_core_limit() {
#ifndef _WIN32
#ifndef NDEBUG
#define KGL_CORE_DUMP_LIMIT 1073741824
	struct rlimit rlim;
	if (0 == getrlimit(RLIMIT_CORE, &rlim)) {
		if (rlim.rlim_cur < KGL_CORE_DUMP_LIMIT && rlim.rlim_cur != RLIM_INFINITY) {
			//turn on coredump
			//rlim.rlim_cur = RLIM_INFINITY;
			//rlim.rlim_max = RLIM_INFINITY;
			rlim.rlim_cur = KGL_CORE_DUMP_LIMIT;
			rlim.rlim_max = KGL_CORE_DUMP_LIMIT;
			int ret = setrlimit(RLIMIT_CORE, &rlim);
			klog(KLOG_ERR, "set core dump limit ret=[%d]\n", ret);
		} else {
			klog(KLOG_ERR, "core dump  limit [%lld %lld] will not set.\n", (long long)rlim.rlim_cur, (long long)rlim.rlim_max);
		}
	} else {
		klog(KLOG_ERR, "get core dump  limit error [%d]\n", errno);
	}
#endif
#endif
}
bool init_resource_limit(int numcpu) {
	bool result = true;
#ifndef _WIN32
	//adjust max open file
	struct rlimit rlim;
	unsigned open_file_limited = 65536 * numcpu;
	if (open_file_limited < 65536) {
		open_file_limited = 65536;
	}
	if (0 == getrlimit(RLIMIT_NOFILE, &rlim)) {
		if (rlim.rlim_max < open_file_limited) {
			rlim.rlim_cur = open_file_limited;
			rlim.rlim_max = open_file_limited;
			int ret = setrlimit(RLIMIT_NOFILE, &rlim);
			if (ret != 0) {
				klog(KLOG_ERR, "set open file limit error [%d]\n", errno);
				result = false;
			}
		}
	}
	if (0 == getrlimit(RLIMIT_NOFILE, &rlim)) {
		klog(KLOG_ERR, "max open file limit [cur:%d,max:%d]\n", rlim.rlim_cur, rlim.rlim_max);
		open_file_limit = rlim.rlim_max;
	} else {
		klog(KLOG_ERR, "get max open file limit error [%d]\n", errno);
	}
#endif
	return result;
}
#ifdef _WIN32
unsigned getpagesize() {
	SYSTEM_INFO  si;
	GetSystemInfo(&si);
	return si.dwPageSize;
}
#endif
void init_program() {
	kasync_init();
	init_http_server_callback(kangle::kgl_on_new_connection, start_request_fiber);
	selector_manager_init(1, false);
	for (int i = 0; i < 2; i++) {
		kaccess[i] = new KAccess(true, i);
	}
	klog_start();
	KAccess::loadModel();
}

#ifndef _WIN32
int create_worker_process(int index) {
	worker_index = index;
	int pid = fork();
	if (pid == 0) {
		//child
		//singleProgram.unlock();
		std::map<int, WorkerProcess*>::iterator it;
		for (it = workerProcess.begin(); it != workerProcess.end(); it++) {
			delete (*it).second;
		}
		workerProcess.clear();
		/*
		for (size_t i = 0; i < conf.service.size(); i++) {
			delete conf.service[i];
		}
		conf.service.clear();
		*/
	}
	return pid;
}
#endif
void my_fork() {
#ifndef _WIN32
	std::map<int, WorkerProcess*>::iterator it;
	for (;;) {
		if (workerProcess.empty()) {
			if (quit_program_flag > 0) {
				m_pid = 0;
				singleProgram.deletePid();
				my_exit(0);
				break;

			}
			for (size_t i = 0; i < 1; i++) {
				int pid = create_worker_process(i);
				if (pid == 0) {
					return;
				}
				if (pid < 0) {
					continue;
				}
				WorkerProcess* process = new WorkerProcess;
				process->pid = pid;
				process->worker_index = i;
				workerProcess.insert(pair<int, WorkerProcess*>(pid, process));
			}
		}
		int status;
		int pid = waitpid(-1, &status, WNOHANG);
		if (pid <= 0) {
			sleep(1);
			continue;
		}
		it = workerProcess.find(pid);
		if (it == workerProcess.end()) {
			continue;
		}
		WorkerProcess* process = (*it).second;
		clean_process(process->pid);
		if (WEXITSTATUS(status) == 100) {
			shutdown_safe_process();
			exit(0);
		}
		workerProcess.erase(it);
		if (quit_program_flag == PROGRAM_NO_QUIT) {
			pid = create_worker_process(process->worker_index);
			if (pid == 0) {
				return;
			}
			if (pid < 0) {
				fprintf(stderr, "create worker process failed,errno=%d\n", errno);
				continue;
			}
			process->pid = pid;
			workerProcess.insert(pair<int, WorkerProcess*>(pid, process));
		} else {
			delete process;
		}
	}
#endif
}
int main_fiber(void* arg, int argc) {
	//init kxml
	KXml::fopen = (kxml_fopen)kfiber_file_open;
	KXml::fclose = (kxml_fclose)kfiber_file_close;
	KXml::fsize = (kxml_fsize)kfiber_file_size;
	KXml::fread = (kxml_fread)kfiber_file_read;
#ifdef _WIN32
	if (worker_index == 0) {
		create_signal_pipe();
	}
	if (kflike(shutdown_event) || kflike(signal_pipe)) {
		kthread_start(signal_thread, NULL);
	}
	if (worker_index == 0) {
		save_pid();
	}
#endif
	kgl_update_http_time();
#ifdef KSOCKET_SSL
	kssl_set_sni_callback(kgl_lookup_sni_ssl_ctx, kgl_free_ssl_sni);
#endif

	do_config(true);
	m_pid = getpid();
	init_core_limit();
	for (int i = kgl_cpu_number; i > 0; i--) {
		if (init_resource_limit(i)) {
			break;
		}
	}
#ifndef NDEBUG
	chdir(conf.path.c_str());
#endif
	set_user();
#ifndef _WIN32
	my_uid = getuid();
	m_ppid = getppid();
#endif
	klog(KLOG_NOTICE, "Start success [pid=%d].\n", m_pid);
#ifdef _WIN32
	if (m_ppid == 0) {
		m_ppid = getpid();
	}
	init_winuser(true);
#endif
#ifdef ENABLE_VH_RUN_AS	
	conf.gam->loadAllApi();
#endif
#ifdef ENABLE_TF_EXCHANGE
	if (worker_index == 0) {
		kthread_pool_start(clean_tempfile_thread, NULL);
	}
#endif
	assert(test());
	kcond* cond = (kcond*)arg;
	kcond_notice(cond);
	return 0;
}
void StartAll() {
	init_signal();
#ifndef _WIN32
	if (!nodaemon && m_debug == 0) {
		daemon(1, 0);
	}
	save_pid();
	if (!nofork) {
		my_fork();
		singleProgram.unlock();
	}
	signal(SIGCHLD, SIG_IGN);
#endif
#ifdef ENABLE_LOG_DRILL
	init_log_drill();
#endif
	set_program_home_env(conf.path.c_str());
	init_aio_align_size();
	spProcessManage.setName("api:sp");
	initFastcgiData();
	kgl_pagesize = getpagesize();
	server_container = new KCdnContainer;
	init_program();
	auto cond = kcond_init(true);
	selector_manager_on_ready([](KOPAQUE data, void* arg, int got) {
		kfiber_create(main_fiber, arg, 0, 0, nullptr);
		return kev_ok;
		}, cond);
	selector_manager_start(kgl_update_http_time, true);
	kcond_wait(cond);
	//wait fiber init done.
	kcond_destroy(cond);
	time_thread(NULL);
#ifdef MALLOCDEBUG
	checkMemoryLeak();
#endif
	ksocket_clean();
}
void StopAll() {
	shutdown_signal(0);
}

int main(int argc, char** argv) {
#ifdef _WIN32
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif
	srand((unsigned)time(NULL));
	program_rand_value = rand();

	if (!create_path(argv)) {
		fprintf(stderr, "cann't create path,don't start kangle in search path\n");
#ifdef _WIN32
		LogEvent("cann't create path\n");
#endif
		my_exit(0);
	}
	//{{ent
#ifdef _WIN32	
#ifdef _WIN32_SERVICE
	if (argc == 2) {
		if (strcmp(argv[1], "--onlyinstall") == 0) {
			if (InstallService(PROGRAM_NAME, true, false)) {
				printf("install service success\n");
				return 0;
			} else {
				printf("install service failed\n");
				return 1;
			}
		}
		if (strcmp(argv[1], "--start") == 0) {
			if (InstallService(PROGRAM_NAME, false, true)) {
				printf("start service success\n");
				return 0;
			} else {
				printf("start service failed\n");
				return 1;
			}
		}
		if (::strcmp(argv[1], "--stop") == 0) {
			if (UninstallService(PROGRAM_NAME, false)) {
				printf("stop service success\n");
				return 0;
			} else {
				printf("stop service failed\n");
				return 1;
			}
		}
		if (::strcmp(argv[1], "--install") == 0) {
			if (InstallService(PROGRAM_NAME)) {
				printf("install service success\n");
			} else {
				printf("install service failed\n");
			}
			return 0;
		}
		if (::strcmp(argv[1], "--uninstall") == 0) {
			if (UninstallService(PROGRAM_NAME)) {
				printf("uninstall service success\n");
			} else {
				printf("uninstall service failed\n");
			}
			return 0;
		}
		if (::strcmp(argv[1], "--ntsrv") == 0) {
			SERVICE_TABLE_ENTRY service_table_entry[] =
			{
				{	PROGRAM_NAME, serviceMain},
				{	NULL, NULL}
			};
			::StartServiceCtrlDispatcher(service_table_entry);
			return 0;
		}
		if (strcmp(argv[1], "--safe") == 0) {
			start_safe_service();
			return 0;
		}
	}
	if (argc == 1) {
		//	printf("Usage:\n%s --install	install service\n",argv[0]);
		//	printf("%s --uninstall	uninstall service\n",argv[0]);
	}
#endif
	SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);
#endif
	//}}
	kgl_cpu_number = GetNumberOfProcessors();
	//printf("number of cpus %d\n",numberCpu);
	if (kgl_cpu_number <= 0) {
		kgl_cpu_number = 1;
	}
	//	printf("using LANG %s\n",lang);
	if (parse_args(argc, argv)) {
		Usage();
		my_exit(0);
	}
	//{{ent
#ifdef _WIN32
	if (shutdown_event == INVALID_HANDLE_VALUE) {
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
	}
#endif
	//}}
	StartAll();
	return 0;
}

void save_pid() {
	KString path;
#ifdef KANGLE_VAR_DIR
	path = KANGLE_VAR_DIR;
#else
	path = conf.path;
	path += "/var";
#endif
	path += PID_FILE;
	singleProgram.lock(path.c_str());
}
