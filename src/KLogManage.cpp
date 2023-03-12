#include <string.h>
#include "KLogManage.h"

KLogManage logManage;
using namespace std;
void KLogManage::checkRotate(time_t now_time)
{
	lock.Lock();
	for(auto it=logs.begin();it!=logs.end();it++){
		(*it).second->checkRotate(now_time);
	}
	lock.Unlock();
}
KLogElement *KLogManage::refsLogger(KString path,bool &isnew)
{
	KLogElement *logger = NULL;
	lock.Lock();
	auto it=logs.find(path);
	if(it!=logs.end()){
		isnew = false;
		logger = (*it).second;
	}else{
		logger = new KLogElement;
		logger->path = path;
		logger->place = LOG_FILE;
		logs.insert(pair<KString,KLogElement *>(path,logger));
		isnew = true;
	}
	logger->addRef();
	lock.Unlock();
	return logger;
}
void KLogManage::destroy(KLogElement *logger)
{
	lock.Lock();
	int refs = logger->release();
	if (refs == 0) {
		lock.Unlock();
		return;
	}
	if(refs<=1){
		auto it=logs.find(logger->path);
		if (it!=logs.end() && logger == (*it).second) {
			logs.erase(it);
			logger->release();
		}
	}
	lock.Unlock();
}
