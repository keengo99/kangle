#ifndef KTIMER_H
#define KTIMER_H
#include "ksapi.h"
typedef void (WINAPI *timer_func)(void *arg);
void timer_run(timer_func f,void *arg,int msec,unsigned short selector = 0);
#endif
