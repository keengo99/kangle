#ifndef KLOGHANDLE_H
#define KLOGHANDLE_H
#include <string>
#include "KLogElement.h"
#include "KMutex.h"
#include "KDynamicString.h"
#include "utils.h"

class KLogTask
{
public:
	virtual ~KLogTask()
	{
	};
	virtual void handle() = 0;
};

//日志处理
class KLogDealTask : public KLogTask
{
public:
	~KLogDealTask();
	KLogDealTask(const std::string &logHandle,const char *logFile);
	void handle();
private:
	const char * logFile;
	char **arg;
};
//日志维护
class KLogZeroManageTask : public KLogTask
{
public:
	~KLogZeroManageTask();
	KLogZeroManageTask(const char *logFile,unsigned log_day,INT64 log_size)
	{
		//this->path.swap(path);
		char *tpath = getPath(logFile);
		this->path = tpath;
		xfree(tpath);
		this->log_day = log_day;
		this->log_size = log_size;
	}
	void addFile(const char *filename);
	void handle();
private:
	void removeLog(KFileName *file);
	std::string path;
	unsigned log_day;
	INT64 log_size;
	std::list<KFileName *> files;
};
//日志维护和处理
class KLogHandle
{
public:
	KLogHandle();
	~KLogHandle();
	void handle(KLogElement *le,const char *logFile);
	void task();
	void setLogHandle(std::string logHandle)
	{
		lock.Lock();
		this->logHandle = logHandle;
		lock.Unlock();
	}
private:
	void addLogTask(KLogTask *task);
	KMutex lock;
	std::list<KLogTask *> tasks;
	unsigned threads;
	std::string logHandle;
};
extern KLogHandle logHandle;
#endif
