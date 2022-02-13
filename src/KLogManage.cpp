#include <string.h>
//{{ent
#include "KLicense.h"
//}}
#include "KLogManage.h"

KLogManage logManage;
using namespace std;
void KLogManage::checkRotate(time_t now_time)
{
	lock.Lock();
	map<string,KLogElement *>::iterator it;
	for(it=logs.begin();it!=logs.end();it++){
		(*it).second->checkRotate(now_time);
	}
	lock.Unlock();
}
KLogElement *KLogManage::refsLogger(std::string path,bool &isnew)
{
	KLogElement *logger = NULL;
	lock.Lock();
	map<string,KLogElement *>::iterator it=logs.find(path);
	if(it!=logs.end()){
		isnew = false;
		logger = (*it).second;
	}else{
		logger = new KLogElement;
		logger->path = path;
		logger->place = LOG_FILE;
		logs.insert(pair<string,KLogElement *>(path,logger));
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
	if(refs<=0){
		map<string,KLogElement *>::iterator it=logs.find(logger->path);
		assert(it!=logs.end());
		logs.erase(it);
		delete logger;
	}
	lock.Unlock();
}
