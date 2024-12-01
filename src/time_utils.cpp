#include "time_utils.h"
#include "KHttpLib.h"
volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
KMutex timeLock;
extern void shutdown_all();
void kgl_update_http_time()
{
	timeLock.Lock();
	mk1123time(kgl_current_sec, (char *)cachedDateTime, sizeof(cachedDateTime));
	log_request_time(kgl_current_sec,(char *)cachedLogTime, sizeof(cachedLogTime));
	timeLock.Unlock();
	setActive();
	if (unlikely(configReload)) {
		do_config(false);
		configReload = false;
	}
	if (unlikely(quit_program_flag == PROGRAM_QUIT_PREPARE)) {
        shutdown_all();
    }
}
