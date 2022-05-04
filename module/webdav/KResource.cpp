
#include "KResource.h"

KResource::KResource(void)
{
	name = NULL;
	path = NULL;
}

KResource::~KResource(void)
{
	if(name){
		xfree(name);
	}
	if(path){
		xfree(path);
	}
}
void KResource::setName(char *name)
{
	this->name = name;
}
