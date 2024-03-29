#pragma once
#include "global.h"
#include "KStream.h"
#include "KUrlValue.h"
#define MAX_POST_SIZE	8388608 //8m

class KHttpPost
{
public:
	KHttpPost();
	~KHttpPost(void);
	bool init(int totalLen);
	bool addData(const char *data,int len);
	bool readData(KRStream *st);
	bool parse(KUrlValue *uv);
	const char *getString()
	{
		*hot = '\0';
		return buffer;
	}

private:
	bool parseUrlParam(char *param, size_t len, KUrlValue *uv);
	char *hot;	
	char* buffer;
	int totalLen;
};
