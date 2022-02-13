#ifndef KMSESSIONMANAGE_H
#define KMSESSIONMANAGE_H
#include "KSessionCtx.h"
#include "KMutex.h"
class KSessionManager
{
public:
	static KSessionManager *getManager()
	{
		return &session_manager;
	}
	void timer();
	KSessionCtx *refsSession(const char *id);
	void setSession(const char *id,KSessionCtx *ctx);
private:
	static KSessionManager session_manager;
	KSessionManager()
	{
	}
	~KSessionManager()
	{
	}
	KMutex lock;
};
#endif
