#include "KLogHandle.h"
#include "kthread.h"
#include "do_config.h"
#include "directory.h"
#include "KVirtualHost.h"
#include <algorithm>
#include <vector>
#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
using namespace std;
bool compFile(const KFileName * __x, const KFileName * __y) {
	return __x->getLastModified() < __y->getLastModified();
}
KLogHandle logHandle;
KTHREAD_FUNCTION log_task(void *param) {
	KLogHandle *handle = (KLogHandle *)param;
	handle->task();
	KTHREAD_RETURN;
}
int list_log_dir_handle(const char *file,void *param)
{
	if(strcasecmp(file,"kangle.pid")==0){
		//skip the pid file
		return 0;
	}
	KLogZeroManageTask *lzmt = (KLogZeroManageTask *)param;
	lzmt->addFile(file);
	return 0;
}
KLogDealTask::KLogDealTask(const std::string &logHandle,const char *logFile,unsigned uid,unsigned gid)
{
	this->logFile = logFile;
	this->uid = uid;
	this->gid = gid;
	KDynamicString ds;
	//std::map<std::string,std::string> kv;
	//kv["log_file"] = logFile;
	//ds.setKeyValue(&kv);
	auto log_dir = getPath(logFile);
	if (log_dir) {
		ds.addEnv("log_dir",log_dir.get());
	}
	ds.addEnv("log_file",logFile);
#ifdef _WIN32
	ds.addEnv("exe",".exe");
#else
	ds.addEnv("exe","");
#endif
	std::vector<char *> args;
	char *buffer = strdup(logHandle.c_str());
	explode_cmd(buffer,args);
	//explode(logHandle.c_str(), ' ', args);
	arg = new char *[args.size() + 1];
	size_t i = 0;
	for (; i < args.size(); i++) {
		arg[i] = ds.parseString(args[i]).release();
	}
	arg[i] = NULL;
	free(buffer);
}
void KLogDealTask::handle()
{
	Token_t token = NULL;
#ifndef _WIN32
	// 站点日志的处理命令(如 webalizer)以站点身份运行：它的输出目录在站点目录下，
	// 以 root 运行时站点可用软链接让 root 往任意位置写文件。站点日志对站点可读。
	// Site log handlers run as the site: their output lies below the site
	// directory, and the rotated log is readable by the site.
	int site_token[2] = { (int)uid, (int)gid };
	if (uid > 0 || gid > 0) {
		token = site_token;
	}
#endif
	if(!startProcessWork(token,arg,NULL)){
		klog(KLOG_ERR,"Cann't start Log Deal task\n");
	}
}
KLogDealTask::~KLogDealTask()
{
	if(arg){		
		for(int i=0;arg[i];i++){
			xfree(arg[i]);
		}
		delete[] arg;
	}
}
KLogZeroManageTask::~KLogZeroManageTask()
{
	std::list<KFileName *>::iterator it;
	for(it=files.begin();it!=files.end();it++){
		delete (*it);
	}
}
void KLogZeroManageTask::addFile(const char *filename)
{
	KFileName *file = new KFileName;
	if(!file->setName(path.c_str(),filename,FOLLOW_LINK_ALL)){
		delete file;
		return;
	}
	if(file->isDirectory()){
		delete file;
		return;
	}
	files.push_back(file);
}
void KLogZeroManageTask::removeLog(KFileName *file)
{
	int ret = unlink(file->getName());
	klog(KLOG_NOTICE,"remove log file[%s] result=%d\n",file->getName(),ret);
	delete file;

}
#ifndef _WIN32
struct KRotatedLog {
	std::string name;
	time_t mtime;
	INT64 size;
};
static bool rotated_log_older(const KRotatedLog &a, const KRotatedLog &b) {
	return a.mtime < b.mtime;
}
void KLogZeroManageTask::handleSite()
{
	int dfd = klog_open_dir(path.c_str(), false);
	if (dfd < 0) {
		klog(KLOG_ERR, "skip cleaning log dir [%s]: cann't open it without following site links, errno=%d\n", path.c_str(), errno);
		return;
	}
	int list_fd = dup(dfd);
	DIR *dir = list_fd >= 0 ? fdopendir(list_fd) : NULL;
	if (dir == NULL) {
		if (list_fd >= 0) {
			::close(list_fd);
		}
		::close(dfd);
		return;
	}
	std::vector<KRotatedLog> logs;
	INT64 totalSize = 0;
	struct stat st;
	const char *current_base = strrchr(current.c_str(), '/');
	if (current_base && fstatat(dfd, current_base + 1, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(st.st_mode)) {
		// The live log counts toward logs_size but is never removed here.
		totalSize += st.st_size;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (!klog_is_rotated_name(entry->d_name, stem, ext)) {
			continue;
		}
		if (fstatat(dfd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(st.st_mode)) {
			continue;
		}
		KRotatedLog rotated = { entry->d_name, st.st_mtime, (INT64)st.st_size };
		logs.push_back(rotated);
		totalSize += st.st_size;
	}
	closedir(dir);
	std::sort(logs.begin(), logs.end(), rotated_log_older);
	size_t first = 0;
	if (log_day > 0) {
		time_t time_point = time(NULL) - (log_day * 86400);
		for (; first < logs.size() && logs[first].mtime < time_point; first++) {
			int ret = unlinkat(dfd, logs[first].name.c_str(), 0);
			klog(KLOG_NOTICE, "remove log file[%s/%s] result=%d\n", path.c_str(), logs[first].name.c_str(), ret);
			totalSize -= logs[first].size;
		}
	}
	if (log_size > 0) {
		for (; first < logs.size() && totalSize > log_size; first++) {
			int ret = unlinkat(dfd, logs[first].name.c_str(), 0);
			klog(KLOG_NOTICE, "remove log file[%s/%s] result=%d\n", path.c_str(), logs[first].name.c_str(), ret);
			totalSize -= logs[first].size;
		}
	}
	::close(dfd);
}
#endif
void KLogZeroManageTask::handle()
{
#ifndef _WIN32
	if (site) {
		handleSite();
		return;
	}
#endif
	list_dir(path.c_str(),list_log_dir_handle,this);
	files.sort(compFile);
	list<KFileName *>::iterator it;
	if(log_day>0){
		time_t time_point = time(NULL) - ( log_day * 86400);
		for(;;){
			it = files.begin();
			if(it==files.end()){
				break;
			}
			if((*it)->getLastModified() >=time_point){
				break;
			}
			removeLog((*it));
			files.pop_front();
		}
	}
	if(log_size>0){
		INT64 totalSize = 0;
		for(it=files.begin();it!=files.end();it++){
			totalSize += (*it)->get_file_size();
		}
		for(;;){
			if(totalSize <= log_size){
				break;
			}
			it = files.begin();
			if(it==files.end()){
				break;
			}
			totalSize -= (*it)->get_file_size();
			removeLog((*it));
			files.pop_front();

		}
	}
}
KLogHandle::KLogHandle()
{
	threads = 0;
}
KLogHandle::~KLogHandle()
{
}
void KLogHandle::handle(KLogElement *le,const char *logFile)
{
	KLogTask *task = NULL;
	if(le->log_handle){
		lock.Lock();
		if(logHandle.size()>0){
#ifndef _WIN32
			task = new KLogDealTask(logHandle,logFile,le->uid,le->gid);
#else
			task = new KLogDealTask(logHandle,logFile);
#endif
		}
		lock.Unlock();
		if(task){
			addLogTask(task);
		}
	}
	if (worker_index==0 && (le->logs_day>0 || le->logs_size>0)) {
		const char *site_log = NULL;
#ifndef _WIN32
		if (le->uid > 0 || le->gid > 0) {
			site_log = le->path.c_str();
		}
#endif
		task = new KLogZeroManageTask(logFile,le->logs_day,le->logs_size,site_log);
		addLogTask(task);
	}
}
void KLogHandle::addLogTask(KLogTask *task)
{
	unsigned maxLogHandle = conf.maxLogHandle;
	if (maxLogHandle==0) {
		//默认启用20个线程处理
		maxLogHandle = 20;
	}
	bool threadStarted = true;
	lock.Lock();
	tasks.push_back(task);
	if (maxLogHandle>0 && threads>=maxLogHandle) {
		threadStarted = false;
	} else {
		threads++;
	}
	lock.Unlock();
	if (threadStarted) {	
		if (!kthread_pool_start(log_task, this)) {
			// Keep queued work from being stranded when thread creation fails.
			this->task();
		}
	}
}

void KLogHandle::task()
{
	for(;;){
		bool threadClosed = false;
		KLogTask *task = NULL;
		lock.Lock();
		if(tasks.size()>0){
			task = *tasks.begin();
			tasks.pop_front();
		}
		if(task==NULL){
			threads--;
			threadClosed = true;
		}
		lock.Unlock();
		if(threadClosed){
			assert(task==NULL);
			break;
		}
		task->handle();
		delete task;
	}
}
