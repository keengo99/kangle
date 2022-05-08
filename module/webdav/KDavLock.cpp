#include <sstream>
#include "kforwin32.h"
#include "KDavLock.h"

using namespace std;
extern KDavLockManager lockManager;
KDavLockFile::KDavLockFile(const char* name)
{
	this->name = xstrdup(name);
	type = Lock_none;
}
KDavLockFile::~KDavLockFile(void)
{
	if (name) {
		xfree(name);
	}
	assert(locks.empty());
}
KLockToken::KLockToken(const char* token, Lock_type type, int timeout)
{
	this->type = type;
	this->timeout = timeout;
	this->token = xstrdup(token);
	locked_file = NULL;
}
KLockToken::~KLockToken()
{
	if (token) {
		xfree(token);
	}
	if (locked_file) {
		locked_file->release();
	}
}
void KLockToken::bind(KDavLockFile* file)
{
	assert(this->locked_file == NULL);
	this->locked_file = file;
	file->addRef();
}
Lock_op_result KDavLockManager::lock(const char* name, KLockToken* lockToken)
{
	Lock_op_result result = Lock_op_failed;
	mutex.Lock();
	KDavLockFile* file = internalFindLockOnFile(name);
	if (file == NULL) {
		file = new KDavLockFile(name);
		fileLocks.insert(std::pair<char*, KDavLockFile*>(file->getName(), file));
	}
	if (file->addLockToken(lockToken)) {
		result = Lock_op_success;
		lockToken->bind(file);
	} else {
		result = Lock_op_conflick;
	}
	assert(!file->is_empty());
	mutex.Unlock();
	return result;
}
KLockToken* KDavLockManager::new_token(const char* owner, Lock_type type, int timeout)
{
	std::stringstream s;
	mutex.Lock();
	s << time(NULL) << "-" << tokenIndex++;
	KLockToken* token = new KLockToken(s.str().c_str(), type, timeout);
	token->addRef();
	lockTokens.insert(pair<char*, KLockToken*>(token->getValue(), token));
	mutex.Unlock();
	return token;
}
Lock_op_result KDavLockManager::unlock(const char* name, KLockToken* lockToken)
{
	auto result = Lock_op_failed;
	mutex.Lock();
	std::map<char*, KDavLockFile*, lessr>::iterator it = fileLocks.find((char*)name);
	if (it == fileLocks.end()) {
		assert(false);
		mutex.Unlock();
		return Lock_op_conflick;
	}
	KDavLockFile* file = (*it).second;	
	if (file->removeLockToken(lockToken)) {
		result = Lock_op_success;
	}
	if (file->locks.empty()) {
		fileLocks.erase(it);
		file->release();
	}
	mutex.Unlock();
	return result;
}
KLockToken* KDavLockManager::find_lock_token(const char* token)
{
	mutex.Lock();
	KLockToken* lockToken = internalFindLockToken(token);
	if (lockToken) {
		lockToken->addRef();
	}
	mutex.Unlock();
	return lockToken;
}
KLockToken *KDavLockManager::find_file_locked(const char* name)
{
	KLockToken* token = NULL;
	mutex.Lock();
	KDavLockFile* davLock = internalFindLockOnFile(name);
	if (davLock) {
		assert(!davLock->locks.empty());
		auto it = davLock->locks.begin();
		if (it != davLock->locks.end()) {
			token = (*it).second;
			token->addRef();
		}
	}
	mutex.Unlock();
	return token;
}
void KDavLockManager::flush(time_t nowTime)
{
	mutex.Lock();
	for (auto it = lockTokens.begin(); it != lockTokens.end(); ) {
		KLockToken* token = (*it).second;
		if (token->isExpire(nowTime)) {			
			auto locked_file = token->locked_file;
			assert(token->locked_file);
			bool result = locked_file->removeLockToken(token);
			assert(result);
			if (locked_file->locks.empty()) {
				auto it2 = this->fileLocks.find(token->locked_file->getName());
				assert(it2 != this->fileLocks.end());
				fileLocks.erase(it2);
				locked_file->release();
			}
			token->release();
			it = lockTokens.erase(it);
			continue;			
		}
		it++;	
	}
	mutex.Unlock();
}
KLockToken* KDavLockManager::internalFindLockToken(const char* token)
{
	map<char*, KLockToken*, lessp>::iterator it = lockTokens.find((char*)token);
	if (it == lockTokens.end()) {
		return NULL;
	}
	return (*it).second;
}
KDavLockFile* KDavLockManager::internalFindLockOnFile(const char* name)
{
	map<char*, KDavLockFile*, lessr>::iterator it = fileLocks.find((char*)name);
	if (it == fileLocks.end()) {
		return NULL;
	}
	return (*it).second;
}
