#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#endif
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "KProcess.h"
#include "do_config.h"
#include "log.h"
#include "utils.h"
#include "kmalloc.h"
//{{ent
#ifdef ENABLE_ADPP
ULONG64 KProcess::lastQueryTime = 0;
#endif
//}}
int kgl_cpu_number = 1;
#ifdef SOLARIS
int GetNumberOfProcessors()
{
	return 2;
}
#endif
#ifdef _WIN32
typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
int GetNumberOfProcessors()
{
	SYSTEM_INFO si;
	// Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.
	PGNSI pfnGNSI = (PGNSI) GetProcAddress(GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo");
	if(pfnGNSI)
	{
		pfnGNSI(&si);
	}
	else
	{
		GetSystemInfo(&si);
	}
	return si.dwNumberOfProcessors;
}
//{{ent
#ifdef ENABLE_ADPP
ULONG64 getFileTime(FILETIME &ft)
{
	ULARGE_INTEGER tm;
	tm.HighPart = ft.dwHighDateTime;
	tm.LowPart = ft.dwLowDateTime;
	return tm.QuadPart;
}
#endif
//}}
#endif
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
int GetNumberOfProcessors()
{
	int ncpus = 1;
	int mib[2];
	size_t len = sizeof(ncpus);
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &ncpus, &len, NULL, 0) == 0) {
         	return ncpus;
	}
	return 1;
}
//{{ent
#ifdef ENABLE_ADPP
static ULONG64 convert_time(char *str)
{
	ULONG64 t = (ULONG64)atoi(str);
	t*=1000000;
	char *p = strchr(str,',');
	if(p) {
		t+=atoi(p+1);
	}
	return t;
}
ULONG64 get_process_time(pid_t pid)
{
	char stat[20];
	char buffer[1024];
	sprintf(stat,"/proc/%d/status",pid);
	FILE *f=fopen(stat,"r");
	if (f==NULL) {
		debug("cann't open [%s] to read,no mount procfs?\n",stat);
		return -1;
	}
	fgets(buffer,sizeof(buffer),f);
	fclose(f);
	char *p=buffer;
	int sp=8;
	while (sp-- && p)
	p=(char *)memchr(p+1,' ',sizeof(buffer)-(p-buffer));
	ULONG64 utime=convert_time(p+1);
	p=(char *)memchr(p+1,' ',sizeof(buffer)-(p-buffer));
	ULONG64 ktime=convert_time(p+1);
	return utime+ktime;
}
#endif
//}}
#endif
#ifdef LINUX
int GetNumberOfProcessors()
{
	int ncpus = -1;
#if defined(_SC_NPROCESSORS_ONLN)
	/* Linux, Solaris, Tru64, UnixWare 7, and Open UNIX 8  */
	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
	/* IRIX */
	ncpus = sysconf(_SC_NPROC_ONLN);
#else
#warning "Please port this function"
#endif

	if (ncpus == -1) {
		return 1;
	}
	if(ncpus==0){
		return 1;
	}
	return ncpus;
}
//{{ent
#ifdef ENABLE_ADPP
int get_jiffies(int pid) {
	char stat[20];
	char buffer[1024];
	sprintf(stat, "/proc/%d/stat", pid);
	FILE *f = fopen(stat, "r");
	if (f == NULL)
		return -1;
	fgets(buffer, sizeof(buffer), f);
	fclose(f);
	char *p = buffer;
	p = (char *) memchr(p + 1, ')', sizeof(buffer) - (p - buffer));
	int sp = 12;
	while (sp--)
		p = (char *) memchr(p + 1, ' ', sizeof(buffer) - (p - buffer));
	//user mode jiffies
	int utime = atoi(p + 1);
	p = (char *) memchr(p + 1, ' ', sizeof(buffer) - (p - buffer));
	//kernel mode jiffies
	int ktime = atoi(p + 1);
	return utime + ktime;
}
ULONG64 get_system_jiffies() {
	ULONG64 nowTime = 0;
	FILE *fp = fopen("/proc/stat", "rb");
	if (fp == NULL) {
		return 0;
	}
	char buf[1024];
	int len = fread(buf, 1, sizeof(buf) - 1, fp);
	if (len <= 0) {
		fclose(fp);
		return 0;
	}
	buf[len] = '\0';
	char *p = strchr(buf, '\n');
	if (p) {
		*p = '\0';
	}
	char *hot = (char *) strchr(buf, ' ');
	for (;;) {
		if (hot == NULL || *hot == '\0') {
			break;
		}
		while (*hot && isspace((unsigned char) *hot)) {
			hot++;
		}
		nowTime += atoi(hot);
		while (*hot && !isspace((unsigned char) *hot)) {
			hot++;
		}
	}
	fclose(fp);
	return nowTime;
}
#endif
//}}
#endif

KProcess::KProcess() {
#ifndef _WIN32
	pid = 0;	
	sig = SIGTERM;
#else
	pid = NULL;
	sig = 0;
#endif
//{{ent
#ifdef ENABLE_ADPP
	lastUsedTime = 0;
	priority = NORMAL_PRIORITY_CLASS;
	cpu_usage = 0;
#endif
//}}
	killed = false;
	file = NULL;
	lastPoweron = time(NULL);
}
KProcess::~KProcess() {
	kill();
#ifdef _WIN32
	if (pid) {
		CloseHandle(pid);
	}
#endif
}
void KProcess::cleanFile()
{
	if (file) {
		unlink(file);
		free(file);
		file = NULL;
	}
}
bool KProcess::saveFile(const char *dir,const char *unix_file)
{
	std::stringstream s;
	s << dir << "kp_" << getpid() << "_" << getProcessId() << "_" << sig;
	kassert(file==NULL);
	file = strdup(s.str().c_str());
	KFile fp;
	if (fp.open(file,fileWrite)) {
		if (unix_file) {
			fp.fprintf("%s",unix_file);
		}
		return true;
	} else {
		free(file);
		file = NULL;
		return false;
	}
}
//{{ent
#ifdef ENABLE_ADPP
ULONG64 KProcess::getCpuTime() {
	ULONG64 nowTime = 0;
#ifdef _WIN32
	FILETIME nowTimef;
	GetSystemTimeAsFileTime(&nowTimef);
	nowTime = getFileTime(nowTimef);
#endif
#ifdef LINUX
	nowTime = get_system_jiffies();
#endif
#ifdef BSD_OS
	nowTime = time(NULL);
#endif
	ULONG64 totalTime = (nowTime - lastQueryTime);
	lastQueryTime = nowTime;
	return totalTime;
}
int KProcess::flushCpuUsage(const KString&user, const KString&name,ULONG64 cpuTime,int cpu_limit)
{
	if (isKilled()) {
		return 0;
	}
	int cpu_usage = getCpuUsage(cpuTime);

	if (cpu_usage > cpu_limit) {
		downPriority();
		klog(KLOG_WARNING,"name=%s,user=%s,pid=%d cpu usage = %d\n",name.c_str(),user.c_str(),getProcessId(),cpu_usage);
		return -1;
	}
	upPriority();
	return 1;
}
int KProcess::getCpuUsage(ULONG64 cpuTime) {
	ULONG64 usedTime = 0;
#ifdef _WIN32
	FILETIME CreationTime,ExitTime,UserTime,KernelTime;
	if(!GetProcessTimes(
					pid,
					&CreationTime,
					&ExitTime,
					&KernelTime,
					&UserTime)
	) {
		return 0;
	}
	usedTime = getFileTime(KernelTime) + getFileTime(UserTime);
#endif
#ifdef LINUX
	usedTime = get_jiffies(pid);
#endif
#ifdef BSD_OS
	usedTime = get_process_time(pid)/1000;
	cpuTime*=1000;
	//printf("usedTime=%lld ms lastUsedTime=%lld,cpuTime=%lld ms\n",usedTime,lastUsedTime,cpuTime);
#endif
	cpu_usage = 0;
	if (lastUsedTime > 0) {
		cpuTime /= 100;
#ifdef _WIN32
		if(kgl_cpu_number>0) {
			cpuTime*=kgl_cpu_number;
		}
#endif
		//printf("usedTime=%I64d,lastUsedTime=%I64d,totalTime=%I64d\n",usedTime,lastUsedTime,totalTime);
		if (cpuTime > 0) {
			cpu_usage = (int) ((usedTime - lastUsedTime) / cpuTime);
			//debug("cpu_percent=%d\n", cpu_usage);
		}
	}
	lastUsedTime = usedTime;
	return cpu_usage;
}
bool KProcess::downPriority() {
	if (priority == IDLE_PRIORITY_CLASS) {
		return false;
	}
	return setPriority(IDLE_PRIORITY_CLASS);
}
bool KProcess::upPriority() {
	if (priority == NORMAL_PRIORITY_CLASS) {
		return false;
	}
	return setPriority(NORMAL_PRIORITY_CLASS);
}
bool KProcess::setPriority(int priority) {
	bool result = false;
#if defined(_WIN32)
	if(SetPriorityClass(pid,priority)) {
		result = true;
	}
#else
	if (setpriority(PRIO_PROCESS, pid, priority) == 0) {
		result = true;
	}
#endif
	if (result) {
		this->priority = priority;
	} else {
		klog(KLOG_ERR, "setpriority failed,not root user?\n");
	}
	return result;
}
#endif
//}}
int KProcess::getProcessId() {
#ifdef _WIN32
	return GetProcessId(pid);
#else
	return pid;
#endif
}
bool KProcess::kill() {
	cleanFile();
	if (killed) {
		return true;
	}
	killed = true;
	bool result = true;
#ifndef _WIN32
	if (pid > 0) {
		if (sig == 0) {
			sig = SIGTERM;
		}
		debug("now kill %d to Child pid=%d\n", sig,pid);
		result = (::kill(pid, sig)==0);
		pid = 0;
	}
#else
	if (pid) {
		result = (TerminateProcess(pid,sig)==TRUE);
		CloseHandle(pid);
		pid = NULL;
	}
#endif
	return result;
}
#ifdef _WIN32
bool KProcess::bindProcessId(DWORD id)
{
	return false;
}
#endif
bool KProcess::isActive()
{
#ifndef _WIN32
	if(pid==0){
		return false;
	}
	bool result = (0 == ::kill(pid,0));
#else
	if(pid==NULL){
		return false;
	}
	bool result = (WAIT_TIMEOUT == WaitForSingleObject(pid,0));
#endif
	if(!result){
		klog(KLOG_ERR,"process pid=[%d] is crashed\n",getProcessId());
	}
	return result;
}
