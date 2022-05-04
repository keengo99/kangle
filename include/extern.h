#if	!defined(_EXTERN_H_INCLUDED_)
#define _EXTERN_H_INCLUDED_
#include <string.h>
#include "global.h"
#include "kforwin32.h"
extern volatile int quit_program_flag;
extern volatile int32_t mark_module_shutdown;
extern int my_uid;
extern int api_child_key;
extern int serial;

void my_exit(int code);
void shutdown_signal(int sig);
void shutdown();
void check_graceful_shutdown();
void reloadVirtualHostConfig();
int stop(int service);
void sigcatch(int sig);
void service_from_signal();
void save_pid();
int create_file_path(char **argv);
void restore_pid();
int parse_args(int argc, char ** argv);
void init_daemon();
void init_program();
int start(int service);
int main(int argc, char **argv);
int forward_signal(const char *protocol);
int get_service(const char * service);
const char * get_service_name(int service);
int get_service_id(const char * service);
void set_user(const char *user);
void console_call_reboot();
void clean_process(int pid);
void init_safe_process();

extern int m_pid;
extern int m_ppid;
extern bool dump_memory_object;
extern volatile bool cur_config_ext;
extern volatile bool cur_config_vh_db;
extern int worker_index;
extern unsigned total_connect;
#ifdef ENABLE_VH_FLOW
extern volatile bool flushFlowFlag;
#endif
#ifdef _WIN32
void WINAPI serviceMain(DWORD argc, LPTSTR * argv);
bool InstallService(const char * szServiceName, bool install = true, bool start = true);
bool UninstallService(const char * szServiceName, bool uninstall = true);
void LogEvent(LPCTSTR pFormat, ...);
void Start();
void Stop();
#endif
struct lessp {
	bool operator()(const char * __x, const char * __y) const {
		return strcmp(__x, __y) < 0;
	}
};
struct lessf {
	bool operator()(const char * __x, const char * __y) const {
		return filecmp(__x, __y) < 0;
	}
};
#endif	/* !_EXTERN_H_INCLUDED_ */
