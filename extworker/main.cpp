#include "kforwin32.h"
#include <stdarg.h>
#include <map>
#include <stdio.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include <Objbase.h>
#endif
#include "ksocket.h"
#include "api_child.h"
#include "extworker.h"
#include "KListenPipeStream.h"
#include "KChildListen.h"
int argc;
char** argv;
int m_debug = 0;
extern std::map<pid_t, time_t> processes;
extern KMutex processLock;
extern std::map<u_short, KApiDso*> apis;
volatile bool program_quit = false;
extern KListenPipeStream ls;
void debug(const char* fmt, ...) {
#ifndef NDEBUG	
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
#endif
}
void killallProcess() {
#ifndef _WIN32
	signal(SIGCHLD, SIG_IGN);
#endif
	program_quit = true;
	debug("now kill all processes program_quit=%d\n", program_quit);
	processLock.Lock();
	std::map<pid_t, time_t>::iterator it;
	for (it = processes.begin(); it != processes.end(); it++) {
#ifdef _WIN32
		TerminateProcess((*it).first, 0);
#else
		kill((*it).first, SIGKILL);
#endif
	}
	processLock.Unlock();
	std::map<u_short, KApiDso*>::iterator it2;
	for (it2 = apis.begin(); it2 != apis.end(); it2++) {
		(*it2).second->unload();
	}
	ls.unlink_unix();
	if (cl) {
		cl->unlink_unix();
	}
}
#ifndef _WIN32
bool childExsit() {
	int status;
	int ret, child;
	int rc;
	ret = waitpid(-1, &status, WNOHANG);
	child = ret;
	switch (ret) {
	case 0:
		return false;
	case -1:
		return false;
	default:
		printf("child %d exsit\n", child);
		if (WIFEXITED(status)) {
			fprintf(stderr, "extworker: child exited with: %d\n", WEXITSTATUS(
				status));
			rc = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "extworker: child signaled: %d\n",
				WTERMSIG(status));
			rc = 1;
		} else {
			fprintf(stderr, "extworker: child died somehow: exit status = %d\n",
				status);
			rc = status;
		}
		if (!program_quit) {
			restart_child_process(child);
		}
	}
	return true;


}
void sigcatch(int sig) {
	switch (sig)
	{
	case SIGCHLD:
		while (childExsit());
		break;
	default:
		program_quit = true;
		killallProcess();
		_exit(0);
	}
}
#endif
void seperate_usage();
int main(int argc, char** argv) {
	//printf("extworker started pid=[%d]\n", getpid());
	::argc = argc;
	::argv = argv;
	ksocket_startup();
	kthread_init();
#ifdef _WIN32
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);
#endif
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);
	signal(SIGUSR1, sigcatch);
	signal(SIGUSR2, sigcatch);
	signal(SIGQUIT, sigcatch);
#endif
	if (argc > 1 && strcmp(argv[1], "-h") == 0) {
		seperate_usage();
		return 1;
	}
	if (argc > 4) {
		if (strcmp(argv[1], "-b") == 0) {
			//独立运行模式
			seperate_work_model();
			return 0;
		}
	}
	KPipeStream st;
#ifdef _WIN32
	int pid = GetCurrentProcessId();
	KStringBuf rf(64), wf(64);
	wf << "\\\\.\\pipe\\kangle" << pid << "r";
	rf << "\\\\.\\pipe\\kangle" << pid << "w";
	if (!st.connect_name(rf.c_str(), wf.c_str())) {
		int error = GetLastError();
		//LogEvent("cann't connect to name last error=%d\n",error);
		return 1;
	}
#else
	signal(SIGCHLD, sigcatch);
	st.fd[0] = 4;
	st.fd[1] = 5;
#endif
	for (;;) {
		if (!api_child_process(&st)) {
			break;
		}
	}
	killallProcess();
	_exit(0);
	return 0;
}
