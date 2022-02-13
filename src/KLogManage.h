#ifndef KLOGMANAGE_H
#define KLOGMANAGE_H
#include <map>
#include "KMutex.h"
#include "KLogElement.h"
#include "global.h"

class KLogManage
{
public:
	KLogElement *refsLogger(std::string path,bool &isnew);
	void destroy(KLogElement *logger);
	friend class KVirtualHost;
	void checkRotate(time_t now_time);
private:
	std::map<std::string,KLogElement *> logs;
	KMutex lock;
};
extern KLogManage logManage;
#endif
