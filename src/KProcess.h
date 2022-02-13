#ifndef KPROCESS_H
#define KPROCESS_H
#include "global.h"
#include "kforwin32.h"
#include "KStream.h"
#ifndef _WIN32
//{{ent
#ifdef ENABLE_ADPP
#define NORMAL_PRIORITY_CLASS 0
#define IDLE_PRIORITY_CLASS   20
#endif
//}}
#define ULONG64	              unsigned long long
#endif
/*
子进程类
*/
class KProcess
{
public:
	KProcess();
	~KProcess();
	bool bind(pid_t pid)
	{
		if(this->pid){
			return false;
		}
		this->pid = pid;
		return true;
	}
	pid_t stealPid()
	{
		pid_t val = pid;
#ifndef _WIN32
		pid = 0;
#else
		pid = NULL;
#endif
		return val;
	}
	pid_t getPid()
	{
		return pid;
	}
	time_t getPowerOnTime()
	{
		return lastPoweron;
	}
#ifdef _WIN32
	bool bindProcessId(DWORD id);
#endif
	//把该进程保存到文件，主进程意外退出时由安全进程杀掉
	bool saveFile(const char *dir,const char *unix_file=NULL);
//{{ent
#ifdef ENABLE_ADPP
	/*
	返回 -1,downPriority
	返回 1 upPriority
	*/
	int flushCpuUsage(const std::string &user, const std::string &name,ULONG64 cpuTime,int cpu_limit);

	static ULONG64 getCpuTime();
	/*
		得到该进程的cpu使用率.
	*/
	int getCpuUsage(ULONG64 cpuTime);
	int getLastCpuUsage()
	{
		return cpu_usage;
	}
	int getPriority()
	{
		return priority;
	}
	/*
	下调优先级
	*/
	bool downPriority();
	/*
	上调优先级
	*/
	bool upPriority();
	static ULONG64     lastQueryTime;
	ULONG64     lastUsedTime;
#endif
//}}
	bool isKilled()
	{
		return killed;
	}
	bool isActive();
	int getProcessId();
	bool kill();
	void detach()
	{
		killed = true;
	}
	int sig;
private:
	void cleanFile();
//{{ent
#ifdef ENABLE_ADPP
	int priority;
	int cpu_usage;
	bool setPriority(int priority);
#endif
//}}
private:
	pid_t pid;
	bool killed;
	time_t lastPoweron;
	char *file;
};
#endif
