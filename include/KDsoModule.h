#ifndef KDSOMODULE_H
#define KDSOMODULE_H
#include "kforwin32.h"
#include "ksapi.h"
class KDsoModule
{
public:
	KDsoModule();
	~KDsoModule();
	bool load(const char *file);
	bool isloaded();
	void *findFunction(const char *func);
	bool unload();
	const char *getError();
private:
	HMODULE handle;
};
#endif
