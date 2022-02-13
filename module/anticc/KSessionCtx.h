#ifndef KSESSIONCTX_H
#define KSESSIONCTX_H
#include "KCountable.h"
class KSessionCtx : public KCountableEx
{
public:
	KSessionCtx(const char *id)
	{
		this->id = strdup(id);
		modified = 1;
	}
	bool cmp(const char *name,const char *value)
	{
	};
	const char *get(const char *name)
	{
	};
	void set(int max_age,const char *name,const char *value)
	{
	};
	char *id;
	volatile int modified;
protected:
	~KSessionCtx()
	{
		xfree(id);
	}
};
#endif
