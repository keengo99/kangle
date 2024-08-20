#include <string.h>
#include <string>
#include "KRequestQueue.h"
#include "kthread.h"
#include "kselectable.h"
#include "http.h"
#include "kselector.h"
#include "kmalloc.h"
#ifdef ENABLE_REQUEST_QUEUE
KRequestQueue globalRequestQueue;

#endif
