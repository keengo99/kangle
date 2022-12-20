#ifndef KAPIENV_H
#define KAPIENV_H
#include <map>
#include "KEnvInterface.h"
#include "utils.h"
/*
为isapi扩展提供env支持的类
*/
class KApiEnv : public KEnvInterface
{
public:
	KApiEnv(void);
	~KApiEnv(void);
	bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override;
	bool addEnv(const char* attr, const char* val);
	bool addContentType(const char* contentType);
	bool addContentLength(const char* contentLength);
	const char* getEnv(const char* attr);

	bool getAllHttp(char* buf, int* buf_size);
	bool getAllRaw(KStringBuf& s);
public:
	char* contentType;
	int contentLength;
protected:
	bool addHttpEnv(const char* attr, const char* val);
private:
	std::map<char*, char*, lessp_icase > serverVars;
	std::map<char*, char*, lessp_icase > httpVars;
	const char* getHttpEnv(const char* val);
	const char* getHeaderEnv(const char* val);
private:
	bool addEnv(const char* attr, const char* val, std::map<char*, char*,
		lessp_icase >& vars);
};
#endif
