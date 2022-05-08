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
	KGL_RESULT start(KFastcgiStream<KSocketStream> *st);
	int writeClient(const char *str, int len) ;
	int readClient(char *buf, int len) ;
	bool setStatusCode(const char *status, int len = 0);
	KGL_RESULT map_url_path(const char* url, LPVOID file, LPDWORD file_len) override;
	KGL_RESULT addHeader(const char *attr, int len = 0) override;
	bool execUrl(HSE_EXEC_URL_INFO *urlInfo);
	bool initECB(EXTENSION_CONTROL_BLOCK *ecb);
	KFastcgiStream<KSocketStream> *st;
};
#endif
