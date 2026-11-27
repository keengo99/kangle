#ifndef EXTWORKER_H
#define EXTWORKER_H
#include "kforwin32.h"
extern int argc;
extern char **argv;
#ifndef _WIN32
#include <signal.h>
extern volatile sig_atomic_t program_quit;
#else
extern volatile bool program_quit;
#endif
void restart_child_process(pid_t pid);
#endif
