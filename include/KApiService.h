#ifndef KAPISERVICE_H
#define KAPISERVICE_H
#include "KApiEnv.h"
#include "khttpext.h"
#include "KApiDso.h"
#include "kforwin32.h"
class KApiService
{
public:
	KApiService(KApiDso *dso)
	{
		headSended = false;
		this->dso = dso;
	}
	virtual ~KApiService()
	{

	}
	int writeClient(const char *str, int len, bool async)
	{
		len = writeClient(str, len);
		return len;
	}
	bool start()
	{
		assert(dso);
		if (!initECB(&ecb)) {
			return false;
		}
		dso->HttpExtensionProc(&ecb);
		return true;
	}
	virtual int writeClient(const char *str, int len) = 0;
	virtual int readClient(char *buf, int len) = 0;
	virtual bool setStatusCode(const char *status, int len = 0) = 0;
	virtual bool addHeader(const char *attr, int len = 0) = 0;
	virtual bool initECB(EXTENSION_CONTROL_BLOCK *ecb) = 0;
	virtual Token_t getToken()
	{
		return NULL;
	}
	KApiEnv env;
	bool headSended;
	EXTENSION_CONTROL_BLOCK ecb;
	KApiDso *dso;
};
#endif
