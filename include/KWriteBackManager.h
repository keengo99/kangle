#ifndef KWRITEBACKMANAGER_H_
#define KWRITEBACKMANAGER_H_
#include<vector>
#include<string>
#include "global.h"
#include "KWriteBack.h"
#include "KRWLock.h"
#ifdef ENABLE_WRITE_BACK
class KWriteBackManager {
public:
	KWriteBackManager();
	virtual ~KWriteBackManager();
	void copy(KWriteBackManager &a) {
		lock.WLock();
		writebacks.swap(a.writebacks);
		lock.WUnlock();
	}
	void destroy();
	bool delWriteBack(KString name, KString &err_msg);
	bool addWriteBack(KWriteBack *wb);
	KWriteBack * refsWriteBack(KString name);
	bool newWriteBack(KString name, KString msg, KString &err_msg);
	bool editWriteBack(KString name, KWriteBack &m_a, KString &err_msg);
	KString writebackList(KString name = "");
	std::vector<KString> getWriteBackNames();	
	friend class KAccess;
private:
	KWriteBack * getWriteBack(KString table_name);
	KRWLock lock;
	std::map<KString,KWriteBack *> writebacks;
};
extern KWriteBackManager writeBackManager;
#endif
#endif /*KWRITEBACKMANAGER_H_*/
