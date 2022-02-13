#ifndef KCHILDAPISERVICE_H
#define KCHILDAPISERVICE_H
#include "KApiService.h"
#include "KFastcgiUtils.h"
#include "KSocketStream.h"

class KChildApiService : public KApiService
{
public:
	KChildApiService(KApiDso *dso);
	~KChildApiService();
	bool start(KFastcgiStream<KSocketStream> *st);
	int writeClient(const char *str, int len) ;
	int readClient(char *buf, int len) ;
	bool setStatusCode(const char *status, int len = 0);
	bool addHeader(const char *attr, int len = 0);
	bool execUrl(HSE_EXEC_URL_INFO *urlInfo);
	bool initECB(EXTENSION_CONTROL_BLOCK *ecb);
	KFastcgiStream<KSocketStream> *st;
	char *post_data;
};
#endif
