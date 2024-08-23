#ifndef KBIGOBJECT_H
#define KBIGOBJECT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "kselector.h"
#include "kfile.h"
#include "ksapi.h"
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
class KHttpRequest;
KGL_RESULT turn_on_bigobject(KHttpRequest* rq, KHttpObject* obj, kgl_response_body *body);
#endif
#endif
