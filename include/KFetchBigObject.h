#ifndef KFETCHBIGOBJECT_H
#define KFETCHBIGOBJECT_H
#include "KFetchObject.h"
#include "kselector.h"
#include "kasync_file.h"
#include "http.h"
#include "kfiber.h"
#define MIN_SENDFILE_SIZE 4096

#ifdef ENABLE_BIG_OBJECT
kfiber_file* kgl_open_big_file(KHttpRequest* rq, KHttpObject* obj, INT64 start);
#endif

#endif
