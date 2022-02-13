#ifndef EXTWORKER_H
#define EXTWORKER_H
#include "kforwin32.h"
extern int argc;
extern char **argv;
extern volatile bool program_quit;
void restart_child_process(pid_t pid);
#endif
