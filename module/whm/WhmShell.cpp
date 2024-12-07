#include "WhmShell.h"
#include "WhmShellSession.h"

KTHREAD_FUNCTION whmShellAsyncThread(void* param) {
	WhmShellContext* sc = (WhmShellContext*)param;
	WhmShell* shell = sc->shell;
	assert(shell);
	shell->asyncRun(sc);
	KTHREAD_RETURN;
}
WhmShell::WhmShell() {
	process = last = NULL;
	curProcess = NULL;
	head = end = NULL;
	index = 0;
	merge_context = NULL;
	merge_context_running = false;
}
WhmShell::~WhmShell() {
	while (process) {
		last = process->next;
		delete process;
		process = last;
	}
	if (curProcess) {
		delete curProcess;
	}
}
void WhmShell::flush() {
	lock.Lock();
	while (head && kgl_current_sec - head->closedTime > 30) {
		context.erase(head->session);
		WhmShellContext* sc = head;
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
void WhmShell::readStdin(WhmShellContext* sc, WhmContext* context) {
	//查找标准输入数据
	kgl::string stdin_str;
	if (context->getUrlValue()->get("-", stdin_str)) {
		sc->in_buffer.write_all(stdin_str.c_str(), stdin_str.size());
	}
}
void WhmShell::initContext(WhmShellContext *sc,WhmContext *context)
{	
	sc->shell = this;
	sc->async = async;
	sc->attribute = context->getUrlValue()->attribute;	
	sc->bindVirtualHost(context->getVh());
	KStringBuf s;
	s << this->name << ":" << (int64_t)time(NULL) << "_" << (int64_t)sc << "_" << index++;
	sc->session = s.str();
	readStdin(sc,context);
}
int WhmShell::call(const char *callName,const char *eventType,WhmContext *context)
{
	WhmShellContext *sc = NULL;
	if (merge) {
		//合并模式
		int ret = WHM_PROGRESS;
		lock.Lock();
		bool need_start_thread = false;
		if (merge_context==NULL) {
			sc = new WhmShellContext;
			initContext(sc,context);
			sc->addRef();
			this->context.insert(std::pair<KString,WhmShellContext *>(sc->session,sc));
			context->add("session",sc->session);
			if (merge_context_running) {
				//已经在运行
				merge_context = sc;
			} else {
				//启动线程
				need_start_thread = true;
				merge_context_running = true;
				
			}
		} else {
			sc = merge_context;
			readStdin(merge_context,context);
			context->add("session",merge_context->session);
			if (!merge_context_running) {
				//启动线程
				need_start_thread = true;
				merge_context_running = true;
				merge_context = NULL;
		
			}
		}
		lock.Unlock();
		if (need_start_thread) {			
			if (!kthread_pool_start(whmShellAsyncThread,sc)) {
				removeContext(sc);
				lock.Lock();
				merge_context_running = false;
				lock.Unlock();
				sc->release();
				context->setStatus("cann't start thread");
				ret = WHM_CALL_FAILED;
			}
		}
		return ret;
	}
	//普通模式
	sc = new WhmShellContext;
	initContext(sc,context);
	if (!async) {
		run(sc);
		int ret = result(sc,context);
		delete sc;
		return ret;
	}
	addContext(sc);
	auto session = sc->session;
	if (!kthread_pool_start(whmShellAsyncThread, sc)) {
		removeContext(sc);
		sc->release();
		context->setStatus("cann't start thread");
		return WHM_CALL_FAILED;
	}
	context->add("session",session);
	return WHM_PROGRESS;
}
int WhmShell::result(WhmShellContext *sc,WhmContext *context)
{
	int ret = WHM_PROGRESS;
	if (sc->closed) {
		ret = WHM_OK;
		context->add("errno",sc->last_error);
	}
	sc->lock.Lock();
	KStringBuf s;
	kbuf *buf = sc->out_buffer.getHead();
	while (buf && buf->used>0) {
		s.write_all(buf->data,buf->used);
		buf = buf->next;
	}
	sc->out_buffer.destroy();
	sc->lock.Unlock();
	context->add("out",s.c_str(),false);
	return ret;
}
void WhmShell::run(WhmShellContext *sc)
{
	WhmShellProcess *p = process;
	while (p) {
		p->run(sc);
		p = p->next;
	}
	//set the shellcontext is closed
	sc->closed = true;
}
bool WhmShell::startElement(KXmlContext *context)
{
	if (context->qName=="commands") {
		if (curProcess==NULL) {
			curProcess = new WhmShellProcess;
			auto v = context->attribute["stdin"];
			if (v.size()>0) {
				curProcess->stdin_file = strdup(v.c_str());
			}
			v = context->attribute["stdout"];
			if (v.size()>0) {
				curProcess->stdout_file = strdup(v.c_str());
			}
			v = context->attribute["stderr"];
			if (v.size()>0) {
				curProcess->stderr_file = strdup(v.c_str());
			}
			v = context->attribute["runas"];
			if (v=="system") {
				curProcess->runAsUser = false;
			} else {
				curProcess->runAsUser = true;
			}
			v = context->attribute["curdir"];
			if (v.size()>0) {
				curProcess->curdir = strdup(v.c_str());
			}
		}
	}
	if (context->qName=="extend") {
		async = (context->attribute["async"]=="1");
		merge = (context->attribute["merge"]=="1");
	}
	return true;
}
bool WhmShell::startCharacter(KXmlContext *context, char *character, int len)
{
	if (context->qName=="command" && curProcess) {
		curProcess->addCommand(character);
	}
	return true;
}
bool WhmShell::endElement(KXmlContext *context)
{
	if (context->qName=="commands" && curProcess) {
		addProcess(curProcess);
		curProcess = NULL;
	}
	return true;
}
void WhmShell::addProcess(WhmShellProcess *p)
{
	if (last==NULL) {
		process = last = p;
	} else {
		last->next = p;
		last = p;
	}
	assert(last->next==NULL);
}
void WhmShell::include(WhmShell *shell)
{
	WhmShellProcess *p = shell->process;
	while (p) {
		addProcess(p->clone());
		p = p->next;
	}
}
void WhmShell::endContext(WhmShellContext *sc)
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
void WhmShell::addContext(WhmShellContext *sc)
{
	lock.Lock();	
	context.insert(std::pair<KString,WhmShellContext *>(sc->session,sc));
	lock.Unlock();
	sc->addRef();
}
WhmShellContext *WhmShell::refsContext(KString session)
{
	WhmShellContext *sc = NULL;
	lock.Lock();
	auto it = context.find(session);
	if (it!=context.end()) {
		sc = (*it).second;
		sc->addRef();
	}
	lock.Unlock();
	return sc;
}
bool WhmShell::removeContext(WhmShellContext *sc)
{
	bool result = false;
	lock.Lock();
	assert(sc != merge_context);
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
	std::map<KString,WhmShellContext *>::iterator it;
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
void WhmShell::asyncRun(WhmShellContext *sc)
{
	while (sc) {
		run(sc);
		endContext(sc);
		sc->release();
		if (!merge) {
			break;
		}
		lock.Lock();
		assert(merge_context_running);
		if (merge_context) {
			sc = merge_context;
			merge_context = NULL;
		} else {
			sc = NULL;
			merge_context_running = false;
		}
		lock.Unlock();
	}
}
