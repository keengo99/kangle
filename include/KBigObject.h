#ifndef KBIGOBJECT_H
#define KBIGOBJECT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "KHttpRequest.h"
#include "kselector.h"
#include "kfile.h"
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
KGL_RESULT turn_on_bigobject(KHttpRequest* rq, KHttpObject* obj, kgl_response_body *body);
#endif
#endif
