#ifndef TIMEUTILS_H
#define TIMEUTILS_H
#include "KMutex.h"
#include "extern.h"
#include "kserver.h"
#include "KConfig.h"
#ifndef _WIN32
#include <sys/time.h>
#endif
int make_http_time(time_t time,char *buf,int size);
const char *log_request_time(time_t time,char *tstr,size_t buf_size);
extern volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
extern volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
extern volatile bool configReload;

extern KMutex timeLock;
//{{ent
#ifdef WIN32
extern HANDLE active_event;
#endif//}}
inline void setActive()
{
	//{{ent
#ifdef _WIN32
#ifdef ENABLE_DETECT_WORKER_LOCK
	if (active_event) {
		SetEvent(active_event);
	}
#endif
#endif//}}
}
void kgl_update_http_time();
#endif