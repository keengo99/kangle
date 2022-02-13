#include "KLogHandle.h"
#include "kthread.h"
#include "do_config.h"
#include "directory.h"
#include "KVirtualHost.h"
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
KLogDealTask::KLogDealTask(const std::string &logHandle,const char *logFile)
{
	this->logFile = logFile;
	KDynamicString ds;
	//std::map<std::string,std::string> kv;
	//kv["log_file"] = logFile;
	//ds.setKeyValue(&kv);
	char *log_dir = getPath(logFile);
	if (log_dir) {
		ds.addEnv("log_dir",log_dir);
		free(log_dir);
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
		arg[i] = ds.parseString(args[i]);
	}
	arg[i] = NULL;
	free(buffer);
}
void KLogDealTask::handle()
{
	if(!startProcessWork(NULL,arg,NULL)){
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
void KLogZeroManageTask::handle()
{
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
			totalSize += (*it)->fileSize;
		}
		for(;;){
			if(totalSize < log_size){
				break;
			}
			it = files.begin();
			if(it==files.end()){
				break;
			}
			totalSize -= (*it)->fileSize;
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
			task = new KLogDealTask(logHandle,logFile);
		}
		lock.Unlock();
		if(task){
			addLogTask(task);
		}
	}
	if (worker_index==0 && (le->logs_day>0 || le->logs_size>0)) {
		task = new KLogZeroManageTask(logFile,le->logs_day,le->logs_size);
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
			lock.Lock();
			threads--;
			lock.Unlock();
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

