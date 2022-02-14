#ifndef KBIGOBJECT_H
#define KBIGOBJECT_H
#include "global.h"
#include <list>
#include "krbtree.h"
#include "KHttpRequest.h"
#include "kselector.h"
#include "kfile.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
class KHttpObject;
//已完成大物件
class KBigObject
{
public:
	KBigObject()
	{
		bodyStart = 0;
	}
	~KBigObject()
	{
	}
	void setBodyStart(int bodyStart)
	{
		this->bodyStart = bodyStart;
	}
	int getBodyStart()
	{
		return bodyStart;
	}
protected:
	int bodyStart;
};
KGL_RESULT turn_on_bigobject(KHttpRequest* rq, KHttpObject* obj);
KGL_RESULT handle_bigobject_progress(KHttpRequest *rq,KHttpObject *obj);
#endif
//}}
#endif
