//{{ent
#include "KWinCgiEnv.h"
/*
#include "KLogElement.h"
void dump_env(char *env)
{
	errorLogger.startLog();
	errorLogger << "dump envs:\n";
	while(*env){
		errorLogger << env << "\n";
		env += (strlen(env)+1);
	}
	errorLogger.endLog(true);
}
*/
KWinCgiEnv::KWinCgiEnv(void) {
	s = new KStringBuf(2048);
}

KWinCgiEnv::~KWinCgiEnv(void) {
	delete s;
}
bool KWinCgiEnv::add_env(const char* attr, size_t attr_len, const char* val, size_t val_len)
{
	s->write_all(attr, attr_len);
	s->write_all(_KS("="));
	s->write_all(val, val_len);
	s->write_all(_KS("\0"));
	return true;
}

bool KWinCgiEnv::addEnv(const char *attr, const char *val) {
	*s << attr << "=" << val;
	return s->write_all("\0", 1) == STREAM_WRITE_SUCCESS;
}
char *KWinCgiEnv::dump_env() {
	return s->getBuf();
}
bool KWinCgiEnv::addEnv(const char *env) {
	*s << env;
	return s->write_all("\0", 1) == STREAM_WRITE_SUCCESS;

}
bool KWinCgiEnv::addEnvEnd()
{
	return s->write_all("\0", 1) == STREAM_WRITE_SUCCESS;
}
void KWinCgiEnv::dump()
{
	//dump_env(s.getBuf());
}
//}}
