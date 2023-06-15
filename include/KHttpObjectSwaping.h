#ifndef KHTTPOBJECTSWAPING_H
#define KHTTPOBJECTSWAPING_H
#include "global.h"
#include "kasync_file.h"
#include "KBuffer.h"
#include "kfiber.h"
#include "KMutex.h"
enum swap_in_result {
	swap_in_success,
	swap_in_busy,
	swap_in_failed_open,
	swap_in_failed_read,
	swap_in_failed_format,
	swap_in_failed_parse,
	swap_in_failed_clean_blocked,
	swap_in_failed_other,
};
#ifdef ENABLE_DISK_CACHE
class KHttpRequest;
class KHttpObject;
class KHttpObjectBody;
//typedef kev_result (*swap_http_obj_call_back)(KHttpRequest *rq, KHttpObject *obj, swap_in_result result);
class KHttpObjectSwapWaiter {
public:
	//kselector *selector;
	kfiber *fiber;
	KHttpObjectSwapWaiter *next;
};
class KHttpObjectSwaping
{
public:
	KHttpObjectSwaping()
	{
		waiter = NULL;
	}
	~KHttpObjectSwaping()
	{
		assert(waiter == NULL);
	}
	swap_in_result swapin(KHttpRequest *rq, KHttpObject *obj);
	swap_in_result wait(KMutex *lock);
private:
	swap_in_result swapin_head(kfiber_file *fp, KHttpObject *obj, KHttpObjectBody *data);
	swap_in_result swapin_head_body(kfiber_file *fp, KHttpObject *obj, KHttpObjectBody *data);
#ifdef ENABLE_BIG_OBJECT_206
	swap_in_result swapin_proress(KHttpObject *obj, KHttpObjectBody *data);
#endif
	void swapin_body_result(KHttpObjectBody *data, char *buf, int got, kbuf **last);
	void notice(KHttpObject *obj, KHttpObjectBody *data, swap_in_result result);
	KHttpObjectSwapWaiter *waiter;
};
#endif
#endif
