#include <sstream>
#include "kforwin32.h"
#include "KDavLock.h"

using namespace std;
KDavLock::KDavLock(const char *name)
{
	this->name = xstrdup(name);
	type = Lock_none;
}
KDavLock::~KDavLock(void)
{
	if(name){
		xfree(name);
	}
}
KLockToken::KLockToken(const char *token,Lock_type type,int timeout)
{
		this->type = type;
		this->timeout = timeout;
		this->token = xstrdup(token);
}
KLockToken::~KLockToken()
{
	if(token){
		xfree(token);
	}
}
void KLockToken::addFile(KDavLock *file)
{
	refsLock.Lock();
	lockedFiles.insert(std::pair<char *,KDavLock *>(file->getName(),file));
	refsLock.Unlock();
}
void KLockToken::removeFile(KDavLock *file)
{
	refsLock.Lock();
	std::map<char *,KDavLock *,lessr>::iterator it=lockedFiles.find(file->getName());
	if(it!=lockedFiles.end()){
		lockedFiles.erase(it);
	}else{
		assert(false);
	}
	refsLock.Unlock();
}
Lock_op_result KDavLockManager::lock(const char *name,KLockToken *lockToken)
{
	Lock_op_result result = Lock_op_failed;
	mutex.Lock();
	KDavLock *file = internalFindLockOnFile(name);
	if(file && file->addLockToken(lockToken)){
		result = Lock_op_success;
		lockToken->addFile(file);
//		lockToken->lockedFile.push_back(file->name);
	}
	mutex.Unlock();
	return Lock_op_success;
}
KLockToken *KDavLockManager::newToken(const char *owner,Lock_type type,int timeout)
{
	std::stringstream s;
	mutex.Lock();
	s << time(NULL) << "-" << tokenIndex++;
	KLockToken *token = new KLockToken(s.str().c_str(),type,timeout);
	token->addRef();
	lockTokens.insert(pair<char *,KLockToken *>(token->getValue(),token));
	mutex.Unlock();
	return token;
}
void KDavLockManager::releaseToken(KLockToken *lockToken)
{
	if(lockToken==NULL){
		return;
	}
	mutex.Lock();
	if(lockToken->release()<=0){
		assert(lockToken->lockedFiles.size()==0);
		lockTokens.erase(lockToken->getValue());
		delete lockToken;
	}
	mutex.Unlock();
}
Lock_op_result KDavLockManager::unlock(const char *name,KLockToken *lockToken)
{
	mutex.Lock();
	std::map<char *,KDavLock *,lessr>::iterator it = fileLocks.find((char *)name);
	if(it==fileLocks.end()){
		assert(false);
		mutex.Unlock();
		return Lock_op_conflick;
	}
	KDavLock  *file = (*it).second;
	lockToken->removeFile(file);
	file->removeLockToken(lockToken);
	if(file->locks.size()==0){
		fileLocks.erase(it);
		delete file;
	}
	mutex.Unlock();
	return Lock_op_success;
}
KLockToken *KDavLockManager::findLockToken(const char *token,const char *owner)
{
	mutex.Lock();
	KLockToken *lockToken = internalFindLockToken(token,owner);
	if(lockToken){
		lockToken->addRef();
	}
	mutex.Unlock();
	return lockToken;
}
bool KDavLockManager::isFileLocked(const char *name)
{
	mutex.Lock();
	KDavLock *davLock = internalFindLockOnFile(name);
	mutex.Unlock();
	return davLock!=NULL;
}
void KDavLockManager::flush(time_t nowTime)
{
	mutex.Lock();

	mutex.Unlock();
}
KLockToken *KDavLockManager::internalFindLockToken(const char *token,const char *owner)
{
	map<char *,KLockToken *,lessp>::iterator it = lockTokens.find((char *)token);
	if(it==lockTokens.end()){
		return NULL;
	}
	return (*it).second;
}
KDavLock *KDavLockManager::internalFindLockOnFile(const char *name)
{
	map<char *,KDavLock *,lessr>::iterator it = fileLocks.find((char *)name);
	if(it==fileLocks.end()){
		return NULL;
	}
	return (*it).second;
}
