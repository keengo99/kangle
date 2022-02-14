#ifndef KREPLACECONTENTFILTER_H
#define KREPLACECONTENTFILTER_H
#include "KHttpStream.h"
#include "KHttpRequest.h"
class KReplaceContentMark;
typedef int(*replaceContentMatchCallBack)(void *param, const char *buf,int len, int *ovector, int overctor_size);
typedef bool (*replaceContentCallBack)(void *param,KRegSubString *sub_string,int *ovector,KStringBuf *st);
typedef void (*replaceContentEndCallBack)(void *param);
class KReplaceContentFilter : public KHttpStream
{
public:
	KReplaceContentFilter();
	~KReplaceContentFilter();
	KGL_RESULT write_all(KHttpRequest *rq, const char *buf, int len);
	KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result);
	void setBuffer(int max_buffer)
	{
		this->max_buffer = max_buffer;
	}
	void setHook(replaceContentCallBack callBack,replaceContentEndCallBack endCallBack, replaceContentMatchCallBack match,void *param)
	{
		this->callBack = callBack;
		this->endCallBack = endCallBack;
		this->match = match;
		this->param = param;
		//this->reg = reg;
	}
private:
	bool writeBuffer(KHttpRequest *rq, const char *str,int len);
	bool dumpBuffer(KHttpRequest *rq);
	char *prevData;
	int prevDataLength;
	bool stoped;
	int max_buffer;
	KBuffer b;
	replaceContentMatchCallBack match;
	replaceContentCallBack callBack;
	replaceContentEndCallBack endCallBack;
	void *param;
};
#endif
