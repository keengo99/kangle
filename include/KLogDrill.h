#ifndef KLOGDRILL_H
#define KLOGDRILL_H
#include "global.h"
#ifdef ENABLE_LOG_DRILL
#include "KStringBuf.h"
class KHttpRequest;
void add_log_drill(KHttpRequest *rq, KStringBuf &s);
void flush_log_drill();
void init_log_drill();
#ifndef NDEBUG
//void check_log_drill();
#endif
#endif
#endif

