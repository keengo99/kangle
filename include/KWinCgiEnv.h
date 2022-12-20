#pragma once
//{{ent
#include "KEnvInterface.h"
#include "KStringBuf.h"
void dump_env(char *buf);
class KWinCgiEnv: public KEnvInterface {
public:
	KWinCgiEnv(void);
	~KWinCgiEnv(void);
	bool addEnv(const char *attr, const char *val) override;
	bool add_env(const char* attr, size_t attr_len, const char* val, size_t val_len) override;
	bool addEnv(const char *env);
	char *dump_env();
	bool addEnvEnd();
	void dump();
private:
	KStringBuf *s;
};
//}}
