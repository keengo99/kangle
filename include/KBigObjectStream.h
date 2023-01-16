#ifndef KBIGOBJECTSTREAM_H
#define KBIGOBJECTSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"
#include "KBigObjectContext.h"
#include "KHttpObject.h"

#ifdef ENABLE_BIG_OBJECT_206
KBigObjectReadContext *get_bigobj_response_body(KHttpRequest *rq, kgl_response_body* body);
#endif
#endif
