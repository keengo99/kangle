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
	// uid/gid: the site owning the log (0 for server logs). The handler
	// (webalizer) then runs as that site, since it writes below the site
	// directory where root could be redirected through planted links.
	KLogDealTask(const std::string &logHandle,const char *logFile,unsigned uid=0,unsigned gid=0);
	void handle();
private:
	const char * logFile;
	char **arg;
	unsigned uid;
	unsigned gid;
};
//日志维护
class KLogZeroManageTask : public KLogTask
{
public:
	~KLogZeroManageTask();
	// site_log: the current log path of a site logger, or NULL for server
	// logs. A site's directory may be replaced by a link, so for sites only
	// rotated copies of that log are removed, through a safely opened directory.
	KLogZeroManageTask(const char *logFile,unsigned log_day,INT64 log_size,const char *site_log=NULL)
	{
		//this->path.swap(path);
		auto tpath = getPath(logFile);
		this->path = tpath.get();
		this->log_day = log_day;
		this->log_size = log_size;
		site = false;
#ifndef _WIN32
		if (site_log) {
			site = true;
			klog_rotated_parts(site_log, stem, ext);
			current = site_log;
		}
#endif
	}
	void addFile(const char *filename);
	void handle();
private:
	void removeLog(KFileName *file);
#ifndef _WIN32
	void handleSite();
#endif
	bool site;
	std::string stem;
	std::string ext;
	std::string current;
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
