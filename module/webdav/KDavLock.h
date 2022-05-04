#ifndef KDAVLOCK_H
#define KDAVLOCK_H
#include <map>
#include <list>
#include "KMutex.h"
#include "KCountable.h"
struct lessp
{
	bool operator()(const char* __x, const char* __y) const {
		return strcmp(__x, __y) < 0;
	}
};
struct lessr
{
	bool operator()(const char* __x, const char* __y) const {
		return filecmp(__x, __y) < 0;
	}
};
enum Lock_type
{
	Lock_none,
	Lock_share,
	Lock_exclusive
};
enum Lock_op_result
{
	Lock_op_success,
	Lock_op_conflick,
	Lock_op_failed
};
class KDavLock;

class KLockToken : public KCountable
{
public:
	KLockToken(const char *token,Lock_type type,int timeout);
	~KLockToken();
	void refresh()
	{
		expireTime = time(NULL) + timeout;
	}
	bool isExpire(time_t nowTime)
	{
		return expireTime<nowTime;
	}
	char *getValue()
	{
		return token;
	}
	Lock_type getType()
	{
		return type;
	}
	void addFile(KDavLock *file);
	void removeFile(KDavLock *file);
	friend class KDavLockManager;

private:
	time_t expireTime;
	char *token;
	Lock_type type;
	int timeout;
	std::map<char *,KDavLock *,lessr> lockedFiles;
};
class KDavLock
{
public:
	KDavLock(const char *name);
	~KDavLock(void);
	bool isExpire(time_t nowTime)
	{
		return expireTime<nowTime;
	}
	Lock_type getLockType()
	{
		return type;
	}
	void refresh()
	{
		expireTime = time(NULL);
	}
	bool addLockToken(KLockToken *lockToken)
	{
		mutex.Lock();
		if((lockToken->getType()==Lock_exclusive && type!=Lock_none) || type==Lock_exclusive){
			mutex.Unlock();
			return false;
		}
		lockToken->addRef();
		type = lockToken->getType();
		locks.insert(std::pair<char *,KLockToken *>(lockToken->getValue(),lockToken));
		mutex.Unlock();
		return true;
	}
	bool removeLockToken(KLockToken *lockToken)
	{
		mutex.Lock();
		std::map<char *,KLockToken *,lessp>::iterator it=locks.find(lockToken->getValue());
		if(it==locks.end()){
			mutex.Unlock();
			return false;
		}
		assert(lockToken == (*it).second);
		locks.erase(it);
		mutex.Unlock();
		lockToken->release();
		return true;
	}
	char *getName()
	{
		return name;
	}
	friend class KDavLockManager;
private:
	Lock_type type;
	char *name;
	time_t expireTime;
	std::map<char *,KLockToken * , lessp> locks;
	KMutex mutex;
};
class KDavLockManager
{
public:
	KDavLockManager()
	{
		tokenIndex = 0;
	}
	KLockToken *newToken(const char *owner,Lock_type type,int timeout);
	void releaseToken(KLockToken *lockToken);
	Lock_op_result lock(const char *name,KLockToken *lockToken);
	Lock_op_result unlock(const char *name,KLockToken *lockToken);
	KLockToken *findLockToken(const char *token,const char *owner);
	bool isFileLocked(const char *name);
	void flush(time_t nowTime);
private:
	KLockToken *internalFindLockToken(const char *token,const char *owner);
	KDavLock *internalFindLockOnFile(const char *name);

	std::map<char *,KLockToken *,lessp> lockTokens;
	std::map<char *,KDavLock *,lessr> fileLocks;
	KMutex mutex;
	unsigned tokenIndex;
};
#endif
