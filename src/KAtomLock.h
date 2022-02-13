#ifndef KATOMLOCK_H
#define KATOMLOCK_H
#include "katom.h"
#ifdef ENABLE_ATOM
class KAtomLock
{
public:
	KAtomLock()
	{
		v = 0;
	}
	inline bool lock()
	{
		return katom_cas((void *)&v,0,1);
	}
	inline bool unlock()
	{
		return katom_cas((void *)&v,1,0);
	}
private:
	volatile int v;
};
#endif
#endif
