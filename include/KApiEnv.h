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
	bool add_content_type(const char* contentType,size_t len) override;
	bool add_content_length(const char* contentLength, size_t len) override;
	const char* getEnv(const char* attr) override;

	bool getAllHttp(char* buf, int* buf_size);
	bool getAllRaw(KStringBuf& s);
public:
	char* contentType;
	int64_t contentLength;
protected:
	bool add_http_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override;
private:
	std::map<char*, char*, lessp_icase > serverVars;
	std::map<char*, char*, lessp_icase > httpVars;
	const char* getHttpEnv(const char* val);
	const char* getHeaderEnv(const char* val);
private:
	bool add_env_vars(const char* attr, const char* val, std::map<char*, char*,
		lessp_icase >& vars);
};
#endif
