#ifndef WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#define WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#include <vector>
#include <string.h>
#include <stdlib.h>
#include<list>
#include<string>
#include "global.h"
#include "KMutex.h"
#include "KJump.h"
#include "KXml.h"
#include "kserver.h"
#include "ksapi.h"
#include "KStringBuf.h"
#include "KHttpHeader.h"
#include "KHttpBufferParser.h"
#include "KConfig.h"
#ifdef ENABLE_WRITE_BACK
class KHttpRequest;
class KFetchObject;
class KWriteBack:public KJump
{
public:
	KWriteBack()
	{
		header = NULL;
	}
	void buildRequest(KHttpRequest *rq, KFetchObject **fo);
	KString getMsg();
	void setMsg(KString msg);

	bool keep_alive;
	int status_code;
	KHttpHeader *header;
	KStringBuf body;
};
#endif
#endif
