#ifndef KACLFACTORYMANAGER_H_
#define KACLFACTORYMANAGER_H_
#include "KMark.h"
#include "KRWLock.h"
#include "KAcl.h"
#include "kmalloc.h"
#include "global.h"
class KModelManager
{
public:
	KModelManager();
	virtual ~KModelManager();
	void addAclModel(KAcl *aclFactory);
	
	std::list<KAcl *> aclFactorys;
	std::list<KMark *> markFactorys;
	static void loadDefaultModel();
};
extern KModelManager modelManager[2];
#endif /*KACLFACTORYMANAGER_H_*/
