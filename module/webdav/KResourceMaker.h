#ifndef KRESOURCEMAKER_H
#define KRESOURCEMAKER_H
#include "KResource.h"

class KResourceMaker
{
public:
	KResourceMaker(void)
	{
	}
	virtual ~KResourceMaker(void)
	{
	}
	virtual KResource *bindResource(const char *name,const char *path) = 0;
	virtual KResource *makeFile(const char *name,const char *path) = 0;
	virtual KResource *makeDirectory(const char *name,const char *path) = 0;
	
};
#endif
