#ifdef _WIN32
#include "global.h"
#include "kforwin32.h"
/////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "utils.h"
#include "kmalloc.h"
#include "extern.h"
#include "do_config.h"
#include "KDynamicListen.h"
#pragma comment(lib, "Advapi32.lib")
/*
#define SZAPPNAME "basicservice"
#define SZSERVICENAME PROGRAM_NAME
#define SZSERVICEDISPLAYNAME PROGRAM_NAME
#define SZDEPENDENCIES ""
*/

//KTHREAD_FUNCTION Start(void *param);
void StartAll();
void StopAll();
int parse_args(int argc,char ** argv);

SERVICE_STATUS ssStatus;
SERVICE_STATUS_HANDLE sshStatusHandle;
extern HANDLE shutdown_event;
HANDLE hEventSource = INVALID_HANDLE_VALUE;
KMutex processLock;
extern std::vector<WorkerProcess *> workerProcess;
void coredump(DWORD pid,HANDLE hProcess,PEXCEPTION_POINTERS pExInfo);
static void getSubKey(std::stringstream &subkey)
{
	subkey << "SOFTWARE\\Bangteng\\" << PROGRAM_NAME ;
}
static void deleteRegister()
{
	std::stringstream subkey;
	getSubKey(subkey);
	LONG ret = RegDeleteKey(HKEY_LOCAL_MACHINE,subkey.str().c_str());
	if(ERROR_SUCCESS != ret){
		//fprintf(stderr,"cann't delete register error=%d\n",ret);
	}
}
static void writeRegister()
{
	std::stringstream subkey;
	getSubKey(subkey);
	HKEY sk;
	DWORD disposition;
	LONG ret = RegCreateKeyEx(HKEY_LOCAL_MACHINE,subkey.str().c_str(),0,NULL,0,KEY_WRITE,NULL,&sk,&disposition);
	if(ERROR_SUCCESS != ret){
		fprintf(stderr,"cann't write register error=%d\n",ret);
		return;
	}
	RegSetValueEx(sk,"path",0,REG_SZ,(const BYTE *)conf.path.c_str(),(DWORD)conf.path.size()+1);
	RegSetValueEx(sk,"version",0,REG_SZ,(const BYTE *)VERSION,(DWORD)strlen(VERSION)+1);
	RegCloseKey(sk);
}
bool InstallService(const char * szServiceName,bool install,bool start)
{
	UninstallService(szServiceName,true);
	SC_HANDLE handle = ::OpenSCManager(NULL, NULL,SC_MANAGER_ALL_ACCESS);
	bool result = true;
	if(handle==NULL){
		int error = GetLastError();
		fprintf(stderr,"cann't open sc manager errno=%d\n",error);
		return false;
	}
	SC_HANDLE hService = NULL;
	if (install) {
		char szFilename[256];
		::GetModuleFileName(NULL, szFilename, 255);
		strcat(szFilename," --ntsrv");
		hService = ::CreateService(handle, szServiceName,
				szServiceName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
				SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, szFilename, NULL,
				NULL, NULL, NULL, NULL);
		if(hService==NULL){
			int error = GetLastError();
			fprintf(stderr,"cann't create service manager errno=%d\n",error);
			return false;
		}
		writeRegister();
		SERVICE_DESCRIPTION tmp;
		memset(szFilename,0,sizeof(szFilename));
	#ifdef HTTP_PROXY
		snprintf(szFilename,255,"%s/%s is a http proxy server",PROGRAM_NAME,VERSION);
	#else
		snprintf(szFilename,255,"%s/%s is a virtual web server",PROGRAM_NAME,VERSION);
	#endif
		tmp.lpDescription=szFilename;
		ChangeServiceConfig2(hService,SERVICE_CONFIG_DESCRIPTION,&tmp);
	} else {
		hService= ::OpenService(handle,szServiceName,SERVICE_ALL_ACCESS);
	}
	if(start){
		char *arg[]={(char *)"--ntsrv",NULL};
		result = StartService(hService,NULL,(LPCSTR *)arg) == TRUE ;
	}
	::CloseServiceHandle(hService);
	::CloseServiceHandle(handle);	
	return result;
}

SERVICE_STATUS servicestatus;
SERVICE_STATUS_HANDLE servicestatushandle;
void create_worker_process(const char *cmd,WorkerProcess *process,int index)
{
	KStringBuf s;
	s << "\"" << conf.program << "\"";
	s << " \"--shutdown\" \"" << (INT64)process->shutdown_event << "\"";
#ifdef ENABLE_DETECT_WORKER_LOCK
	s << " \"--active\" \"" << (INT64)process->active_event << "\"";
#endif
	s << " \"--notice\" \"" << (INT64)process->notice_event << "\"";
	s << " \"--worker_index\" \"" << index << "\"";
	s << cmd;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL bResult = FALSE;
	DWORD code;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb= sizeof(STARTUPINFO);
	si.lpDesktop = TEXT("winsta0\\default");
	bResult = CreateProcess(
	  conf.program.c_str(),              // file to execute
	  (char *)s.c_str(),     // command line
	  NULL,              // pointer to process SECURITY_ATTRIBUTES
	  NULL,              // pointer to thread SECURITY_ATTRIBUTES
	  TRUE,             // handles are not inheritable
	  DETACHED_PROCESS | CREATE_NO_WINDOW ,   // creation flags,   // creation flags
	  NULL,              // pointer to new environment block 
	  NULL,              // name of current directory 
	  &si,               // pointer to STARTUPINFO structure
	  &pi                // receives information about new process
	);
	if (!bResult) {
		code = GetLastError();
		LogEvent("cann't create process,error=%d\n",code);
		Sleep(2000);
		return;
	}
	LogEvent("create worker process success with pid=%d\n",pi.dwProcessId);
	process->hProcess = pi.hProcess;
	process->pid = pi.dwProcessId;
	CloseHandle(pi.hThread);
	process->closed = false;
	process->pending_count = 0;
}
void start_safe_service()
{
	init_safe_process();
#ifndef _WIN32
	//windows 由工作进程来save_pid(),unix由安全进程save_pid
	save_pid();
#endif
	DWORD code;
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE;
	shutdown_event = CreateEvent(&sa,TRUE,FALSE,NULL);
	if (shutdown_event==INVALID_HANDLE_VALUE) {
		code = GetLastError();
		LogEvent("cann't create shutdown event,error=%d",code);
		exit(0);
	}
	KStringBuf s;
	s << " \"--ppid\" \"" << (int)GetCurrentProcessId() << "\"";
#if 0
	std::map<KListenKey,KServer *>::iterator it;
	for (it = dlisten.listens.begin(); it != dlisten.listens.end(); it++) {
		KServer *server = (*it).second;
		HANDLE sockfd = (HANDLE)server->server->get_socket();
		::kfile_close_on_exec((HANDLE)sockfd, false);
		s << " \"";
		s << "--listen:" << getWorkModelName(server->model);
		s << "\" \"" << (INT64)sockfd << "\"";
	}
#endif
	sa.bInheritHandle = TRUE;
	//设置最大工作进程为31个。
	//因为windows的WaitForMultiObjects一次最多处理64个对象。每个进程两个对象(一个是进程handle,一个是notice_event),再加一个shutdown_event.
	//所以最大工作进程为31个。
	unsigned max_worker = 31;
#ifdef ENABLE_DETECT_WORKER_LOCK
	//多一个active_event
	max_worker = 21;
#endif
	for (size_t i=0;i<1;i++) {
		WorkerProcess *process = new WorkerProcess;
		process->shutdown_event = NULL;
		process->notice_event = CreateEvent(&sa,TRUE,FALSE,NULL);
#ifdef ENABLE_DETECT_WORKER_LOCK
		process->active_event = CreateEvent(&sa,FALSE,FALSE,NULL);
#endif
		process->closed = true;
		workerProcess.push_back(process);
	}
	int index = 0;
	bool create_process_sleep = false;
	HANDLE *ev = new HANDLE[3*workerProcess.size() + 1];
	HANDLE *active_ev = new HANDLE[workerProcess.size()];
	for(;;){
		//printf("try start the process now\n");
		std::vector<WorkerProcess *>::iterator it2;
		size_t i=0;
		processLock.Lock();
		for (it2=workerProcess.begin();it2!=workerProcess.end();) {
			WorkerProcess *process = (*it2);
#ifdef ENABLE_DETECT_WORKER_LOCK
			if (!process->closed 
				&& process->pending_count > GC_SLEEP_TIME * 60) {
				//没响应
				LogEvent("worker process pid=%d no active,now terminate it\n",process->pid);
				coredump(process->pid,process->hProcess,NULL);
				TerminateProcess(process->hProcess,0);
				clean_process(process->pid);
				process->closed = true;
				create_process_sleep = false;
			}
#endif
			if (process->closed) {
				if (quit_program_flag == PROGRAM_QUIT_IMMEDIATE) {
					it2 = workerProcess.erase(it2);
					continue;
				} else {
					if (kflike((*it2)->shutdown_event)) {
						CloseHandle((*it2)->shutdown_event);
					}
					(*it2)->shutdown_event = CreateEvent(&sa,TRUE,FALSE,NULL);
					if (create_process_sleep) {
						Sleep(2000);
					}
					create_worker_process(s.c_str(),(*it2),(int)i);
				}
			}
			it2++;
			i++;
		}	
		processLock.Unlock();			
		if (workerProcess.size()==0) {
			SetEvent(shutdown_event);
			return;
		}
		index = 0;
		for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
			ev[index++] = (*it2)->hProcess;
		}
		for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
			ev[index++] = (*it2)->notice_event;
		}
		int ret = WaitForMultipleObjects(index,ev,FALSE,2000);
#ifdef ENABLE_DETECT_WORKER_LOCK
		index = 0;
		for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
			active_ev[index++] = (*it2)->active_event;
			(*it2)->pending_count ++ ;
		}
		for (;;) {
			//检查active event
			int ret2 = WaitForMultipleObjects(index ,active_ev,FALSE,0);
			if (ret2>=WAIT_OBJECT_0 && ret2 < (int)workerProcess.size()) {
				WorkerProcess *process = workerProcess[ret2];
				process->pending_count = 0;
				continue;
			}
			break;
		}
#endif
		if (ret >=(int)workerProcess.size() && ret<2*(int)workerProcess.size() ) {
			//notice_event
			WorkerProcess *process = workerProcess[ret - workerProcess.size()];
			CloseHandle(process->hProcess);
			ResetEvent(process->notice_event);
			process->closed = true;
			for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
				if (!(*it2)->closed) {
					SetEvent((*it2)->shutdown_event);
					CloseHandle((*it2)->hProcess);
					(*it2)->closed = true;				
				}
			}
			create_process_sleep = false;
			continue;
		}
		if (ret >=0 && ret<(int)workerProcess.size()) {
			//如果是程序意外退出
			WorkerProcess *process = workerProcess[ret];	
			if (GetExitCodeProcess(process->hProcess,&code)) {
				LogEvent("main process exit code=%d",code);
				if (code == 100) {
					//license expire
					exit(0);
				}
				if (code==99) {
					//if call reboot,notice all worker process close
					process->closed = true;
					for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
						if (!(*it2)->closed) {
							SetEvent((*it2)->shutdown_event);
							CloseHandle((*it2)->hProcess);
							(*it2)->closed = true;				
						}
					}
					continue;
				} else if(code>0) {
					clean_process(process->pid);
					create_process_sleep = true;
				}
			} else {
				code = GetLastError();
				LogEvent("main process exit but I cann't get the exit code,error=%d\n",code);
			}
			CloseHandle(process->hProcess);
			ResetEvent(process->shutdown_event);
			process->closed = true;
		}
	}
}
void stop_safe_service()
{	
	quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	std::vector<WorkerProcess *>::iterator it2;
	processLock.Lock();
	for (it2=workerProcess.begin();it2!=workerProcess.end();it2++) {
		if((*it2)->shutdown_event){
			SetEvent((*it2)->shutdown_event);
		}
	}
	processLock.Unlock();
	//SetEvent(shutdown_event);
	WaitForSingleObject(shutdown_event,INFINITE);
}
void WINAPI ServiceCtrlHandler(DWORD dwControl)
{
	switch (dwControl)
	{
		case SERVICE_CONTROL_PAUSE:
		servicestatus.dwCurrentState = SERVICE_PAUSE_PENDING;
		//
		// This value need to try a lot to confirm
		// ...
		::SetServiceStatus(servicestatushandle, &servicestatus);

		// not called in this service
		// ...
		servicestatus.dwCurrentState = SERVICE_PAUSED;
		//
		break;

		case SERVICE_CONTROL_CONTINUE:
		servicestatus.dwCurrentState = SERVICE_CONTINUE_PENDING;
		::SetServiceStatus(servicestatushandle, &servicestatus);
		servicestatus.dwCurrentState = SERVICE_RUNNING;
		//
		break;

		case SERVICE_CONTROL_STOP:
		servicestatus.dwCurrentState = SERVICE_STOP_PENDING;
		//
		::SetServiceStatus(servicestatushandle, &servicestatus);
		//
		stop_safe_service();
		servicestatus.dwCurrentState = SERVICE_STOPPED;
		//
		break;

		case SERVICE_CONTROL_SHUTDOWN:
		servicestatus.dwCurrentState = SERVICE_STOP_PENDING;
		::SetServiceStatus(servicestatushandle, &servicestatus);
		stop_safe_service();

		servicestatus.dwCurrentState = SERVICE_STOPPED;

		break;

		case SERVICE_CONTROL_INTERROGATE:
		servicestatus.dwCurrentState = SERVICE_RUNNING;
		break;
	}
	::SetServiceStatus(servicestatushandle, &servicestatus);
}

void WINAPI serviceMain(DWORD argc, LPTSTR * argv)
{
	servicestatus.dwServiceType = SERVICE_WIN32;
	servicestatus.dwCurrentState = SERVICE_START_PENDING;
	servicestatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_CONTROL_SHUTDOWN;
	servicestatus.dwWin32ExitCode = 0;
	servicestatus.dwServiceSpecificExitCode = 0;
	servicestatus.dwCheckPoint = 0;
	servicestatus.dwWaitHint = 0;

	servicestatushandle =
	::RegisterServiceCtrlHandler(PROGRAM_NAME, ServiceCtrlHandler);
	if (servicestatushandle == (SERVICE_STATUS_HANDLE)0)
	{
		return;
	}

	bool bInitialized = false;
	// Initialize the service
	// ...
	parse_args(argc,argv);

	bInitialized = true;

	servicestatus.dwCheckPoint = 0;
	servicestatus.dwWaitHint = 0;
	if (!bInitialized)
	{
		servicestatus.dwCurrentState = SERVICE_STOPPED;
		servicestatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		servicestatus.dwServiceSpecificExitCode = 1;
	}
	else
	{
		servicestatus.dwCurrentState = SERVICE_RUNNING;
	}
	::SetServiceStatus(servicestatushandle, &servicestatus);
	start_safe_service();
	return;
}

void LogEvent(LPCTSTR pFormat, ...)
{
	TCHAR chMsg[512];	
	LPTSTR lpszStrings[1];
	va_list pArg;
	va_start(pArg, pFormat);
	_vstprintf(chMsg, pFormat, pArg);
	va_end(pArg);
	lpszStrings[0] = chMsg;
	if(hEventSource == INVALID_HANDLE_VALUE){
		hEventSource = RegisterEventSource(NULL, PROGRAM_NAME);
	}
	if (hEventSource != INVALID_HANDLE_VALUE)
	{
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (LPCTSTR*) &lpszStrings[0], NULL);
	}
}

bool UninstallService( const char *szServiceName,bool uninstall)
{
	bool result = true;
	SC_HANDLE handle = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SC_HANDLE hService= ::OpenService(handle,szServiceName,SERVICE_ALL_ACCESS);
	SERVICE_STATUS service_status;
	result = ControlService(hService,SERVICE_CONTROL_STOP,&service_status) == TRUE;
	if (uninstall) {
		result=::DeleteService(hService)==TRUE;
	}
	::CloseServiceHandle(hService);
	::CloseServiceHandle(handle);
	if (uninstall) {
		deleteRegister();
	}
	return result;
}
#endif
