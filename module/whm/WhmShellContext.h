#ifndef WHMSHELLCONTEXT_H
#define WHMSHELLCONTEXT_H
/**
* whm shell的上下文
*/
#include <string>
#include <map>
#include "KCountable.h"
#include "KVirtualHost.h"
#include "KSocketBuffer.h"
#include "KDynamicString.h"
#include "KExtendProgram.h"

class WhmShell;
class WhmShellContext : public KCountableEx,public KDynamicString 
{
public:
	WhmShellContext()
	{
		vh = NULL;
		last_error = 0;
		kfinit(last_pid);
		closed = false;
		prev = NULL;
		next = NULL;
		ds = NULL;
	}
	~WhmShellContext()
	{
		if (vh) {
			vh->destroy();
		}
		if(ds){
			delete ds;
		}
#ifdef _WIN32
		if(kflike(last_pid)){
			CloseHandle(last_pid);
		}
#endif
	}
	void bindVirtualHost(KVirtualHost *vh)
	{
		if(this->vh){
			return;
		}
		this->vh = vh;
		if (vh) {
			vh->addRef();
		}
		if(this->vh){
			if(ds==NULL){
				ds = new KExtendProgramString(NULL,this->vh);
			}
		}
	}
	void setLastPid(pid_t pid)
	{
#ifdef _WIN32
		if(kflike(last_pid)){
			CloseHandle(last_pid);
		}
#endif
		last_pid = pid;
	}
	pid_t stealLastPid()
	{
		pid_t tpid = last_pid;
#ifdef _WIN32
		last_pid = NULL;
#endif
		return tpid;
	}
	bool buildValue(const char *name,KStringBuf *s)
	{
		std::map<std::string,std::string>::iterator it;
		if (strncasecmp(name,"vh:",3)==0) {
			name += 3;
			if(ds){
				const char *value = ds->interGetValue(name);
				if (value) {
					*s << value;
				}
			}
			return true;
		}
		if (strncasecmp(name,"vhfile:",7)==0) {
			name += 7;
			if (vh) {
				const char *value = "/";
				if (*name=='$') {
					name ++;
					it = attribute.find(name);
					if (it!=attribute.end()) {
						value = (*it).second.c_str();
					}
				} else {
					value = name;
				}
				char *file = KFileName::concatDir(vh->doc_root.c_str(),value);
#ifdef _WIN32
				KFileName::tripDir3(file,'/');
#endif
				*s << file;
				free(file);
			}
			return true;
		}
		if (strcasecmp(name,"exe")==0) {
#ifdef _WIN32
			*s << ".exe";
#endif
			return true;
		}
		const char *value = getSystemEnv(name);
		if (value) {
			*s << value;
			return true;
		}
		if (strncasecmp(name,"url:",4)==0) {
			name += 4;
		}
		
		it = attribute.find(name);
		if (it!=attribute.end()) {
			*s << (*it).second;
		} 
		return true;
	}
	WhmShell *shell;
	std::map<std::string,std::string> attribute;
	//对应虚拟主机
	KVirtualHost *vh;
	KExtendProgramString *ds;
	//输出缓冲
	KSocketBuffer out_buffer;
	KSocketBuffer in_buffer;
	//KPipeStream st;
	int last_error;
	pid_t last_pid;
	//async session id,valiade in async model
	std::string session;
	//is async model
	bool async;
	bool closed;
	KMutex lock;
	time_t closedTime;
	//end context queue,timeout will auto remove
	WhmShellContext *prev;
	WhmShellContext *next;
};
#endif
