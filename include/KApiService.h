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
	KGL_RESULT start()
	{
		assert(dso);
		if (!initECB(&ecb)) {
			return KGL_ESYSCALL;
		}
		DWORD result = dso->HttpExtensionProc(&ecb);
		switch (result) {
		case HSE_STATUS_SUCCESS:
		case HSE_STATUS_SUCCESS_AND_KEEP_CONN:
			return KGL_OK;
		case HSE_STATUS_PENDING:
			return KGL_ENOT_SUPPORT;
		default:
			return KGL_EUNKNOW;
		}
	}
	virtual int writeClient(const char *str, int len) = 0;
	virtual int readClient(char *buf, int len) = 0;
	virtual bool setStatusCode(const char *status, int len = 0) = 0;
	virtual KGL_RESULT addHeader(const char *attr, int len = 0) = 0;
	virtual bool initECB(EXTENSION_CONTROL_BLOCK *ecb) = 0;
	virtual Token_t getToken()
	{
		return NULL;
	}
	KApiEnv env;
	bool headSended;
	bool responseDenied;
	EXTENSION_CONTROL_BLOCK ecb;
	KApiDso *dso;
};
#endif
