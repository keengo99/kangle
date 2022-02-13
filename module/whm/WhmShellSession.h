#ifndef WHMSHELLSESSION_H
#define WHMSHELLSESSION_H
#include <map>
#include <string>
#include "KMutex.h"
#include "WhmShellContext.h"
#if 0
class WhmShellSession
{
public:
	WhmShellSession()
	{
		head = end = NULL;
		index = 0;
		flush_thread_started = false;
	}
	void addContext(WhmShellContext *sc);
	WhmShellContext *refsContext(std::string session);
	void endContext(WhmShellContext *sc);
	bool removeContext(WhmShellContext *sc);
	void flush();
private:
	bool flush_thread_started;
	KMutex lock;
	unsigned index;
	std::map<std::string,WhmShellContext *> context;
	WhmShellContext *head;
	WhmShellContext *end;
};
extern WhmShellSession whmShellSession;
#endif
#endif
