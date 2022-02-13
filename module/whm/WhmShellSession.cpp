#include "WhmShellSession.h"
#if 0
WhmShellSession whmShellSession;
static KTHREAD_FUNCTION flushThread(void *param)
{
	for (;;) {
		sleep(2);
		whmShellSession.flush();
	}
	KTHREAD_RETURN;
}
void WhmShellSession::addContext(WhmShellContext *sc)
{
	lock.Lock();
	//if (!flush_thread_started) {
	//	flush_thread_started = 	m_thread.start(NULL,flushThread);
	//}
	std::stringstream s;
	s << time(NULL) << "_" << index++;
	sc->session = s.str();
	context.insert(std::pair<std::string,WhmShellContext *>(sc->session,sc));
	lock.Unlock();
	sc->addRef();
}
WhmShellContext *WhmShellSession::refsContext(std::string session)
{
	WhmShellContext *sc = NULL;
	lock.Lock();
	std::map<std::string,WhmShellContext *>::iterator it;
	it = context.find(session);
	if (it!=context.end()) {
		sc = (*it).second;
		sc->addRef();
	}
	lock.Unlock();
	return sc;
}
void WhmShellSession::flush()
{
	lock.Lock();
	while (head && kgl_current_sec - head->closedTime > 30) {
		context.erase(head->session);
		WhmShellContext *sc = head;
		head = head->next;
		if (head) {
			head->prev = NULL;
		} else {
			end = NULL;
		}
		sc->release();
	}
	lock.Unlock();
}
void WhmShellSession::endContext(WhmShellContext *sc)
{
	sc->closedTime = kgl_current_sec;
	lock.Lock();
	if (end==NULL) {
		head = end = sc;
	} else {
		end->next = sc;
		sc->prev = end;
		end = sc;
	}
	lock.Unlock();
}
bool WhmShellSession::removeContext(WhmShellContext *sc)
{
	bool result = false;
	lock.Lock();
	if (sc==head) {
		head = head->next;
	}
	if (sc==end) {
		end = end->prev;
	}
	if (sc->next){
		sc->next->prev = sc->prev;
	}
	if (sc->prev){
		sc->prev->next = sc->next;
	}
	std::map<std::string,WhmShellContext *>::iterator it;
	it = context.find(sc->session);
	if (it!=context.end()) {
		assert(sc==(*it).second);
		(*it).second->release();
		context.erase(it);
		result = true;
	}
	lock.Unlock();
	return result;
}
#endif
