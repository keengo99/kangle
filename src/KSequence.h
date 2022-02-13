#ifndef KSEQUENCE_H_
#define KSEQUENCE_H_
#include "KMutex.h"
#define MAX_SEQUENCE	(1<<30)
class KSequence
{
public:
	KSequence()
	{
		seq = 0;		
	}
	unsigned getNext()
	{
		unsigned ret;
		lock.Lock();
		ret = seq++;
		lock.Unlock();
		return ret;
	}
private:
	KMutex lock;
	unsigned seq;
};
extern KSequence sequence;
#endif /*KSEQUENCE_H_*/
