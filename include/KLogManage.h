#ifndef KLOGMANAGE_H
#define KLOGMANAGE_H
#include <map>
#include "KMutex.h"
#include "KLogElement.h"
#include "global.h"
#include "KStringBuf.h"

class KLogManage
{
public:
	KLogElement *refsLogger(KString path,bool &isnew);
	void destroy(KLogElement *logger);
	friend class KVirtualHost;
	void checkRotate(time_t now_time);
	KLocker get_locker() {
		return KLocker(&lock);
	}
private:
	std::map<KString,KLogElement *> logs;
	KMutex lock;
};
extern KLogManage logManage;
#endif
