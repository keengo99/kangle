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
class KDavLockFile;

class KLockToken: public KAtomCountable
{
public:
	KLockToken(const char* token, Lock_type type, int timeout);

	void refresh()
	{
		expireTime = time(NULL) + timeout;
	}
	bool isExpire(time_t nowTime)
	{
		return expireTime < nowTime;
	}
	char* getValue()
	{
		return token;
	}
	Lock_type getType()
	{
		return type;
	}
	void bind(KDavLockFile* file);
	friend class KDavLockManager;
protected:
	~KLockToken();
private:
	time_t expireTime;
	char* token;
	Lock_type type;
	int timeout;
	KDavLockFile *locked_file;
};
class KDavLockFile : public KAtomCountable
{
public:
	KDavLockFile(const char* name);
	~KDavLockFile(void);
	bool isExpire(time_t nowTime)
	{
		return expireTime < nowTime;
	}
	bool is_empty()
	{
		return locks.empty();
	}
	Lock_type getLockType()
	{
		return type;
	}
	void refresh()
	{
		expireTime = time(NULL);
	}
	bool addLockToken(KLockToken* lockToken)
	{
		if ((lockToken->getType() == Lock_exclusive && type != Lock_none) || type == Lock_exclusive) {
			return false;
		}
		lockToken->addRef();
		type = lockToken->getType();
		locks.insert(std::pair<char*, KLockToken*>(lockToken->getValue(), lockToken));
		return true;
	}
	bool removeLockToken(KLockToken* lockToken)
	{
		std::map<char*, KLockToken*, lessp>::iterator it = locks.find(lockToken->getValue());
		if (it == locks.end()) {
			return false;
		}
		assert(lockToken == (*it).second);
		locks.erase(it);
		lockToken->release();
		return true;
	}
	char* getName()
	{
		return name;
	}
	friend class KDavLockManager;
private:
	Lock_type type;
	char* name;
	time_t expireTime;
	std::map<char*, KLockToken*, lessp> locks;
};
class KDavLockManager
{
public:
	KDavLockManager()
	{
		tokenIndex = 0;
	}
	KLockToken* new_token(const char* owner, Lock_type type, int timeout);
	Lock_op_result lock(const char* name, KLockToken* lockToken);
	Lock_op_result unlock(const char* name, KLockToken* lockToken);
	KLockToken* find_lock_token(const char* token);
	KLockToken *find_file_locked(const char* name);
	void flush(time_t nowTime);
	friend class KLockToken;
private:
	KLockToken* internalFindLockToken(const char* token);
	KDavLockFile* internalFindLockOnFile(const char* name);
	bool internal_remove(KDavLockFile* file)
	{
		auto it = fileLocks.find(file->getName());
		if (it == fileLocks.end()) {
			return false;
		}
		fileLocks.erase(it);
		return true;
	}
	std::map<char*, KLockToken*, lessp> lockTokens;
	std::map<char*, KDavLockFile*, lessr> fileLocks;
	KMutex mutex;
	unsigned tokenIndex;
};
#endif
