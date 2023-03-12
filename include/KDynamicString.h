#ifndef KDYNAMICSTRING_H
#define KDYNAMICSTRING_H
#include <map>
#include <list>
#include <string.h>
#include "KStringBuf.h"
#include "global.h"

const char *getSystemEnv(const char *name);
class KControlCodeBlock
{
public:
	KControlCodeBlock()
	{
		key = NULL;
	}
	int getBlockIndex(const char *name)
	{
		if(key==NULL){
			return -1;
		}
		if(strcasecmp(name,key)==0){
			return index;
		}
		return -1;
	}
	const char *key;
	int index;
	int count;
};
class KDynamicString
{
public:
	KDynamicString();

	virtual ~KDynamicString();
	virtual const char *getValue(const char *name)
	{
		std::map<std::string,std::string>::iterator it;
		it = kv.find(name);
		if(it!=kv.end()){
			return (*it).second.c_str();
		}
		return NULL;
	}
	virtual bool buildValue(const char *name,KWStream *s)
	{
		return false;
	}
	virtual const char *getDimValue(const char *name,int index)
	{
		return NULL;
	}
	virtual int getDimSize(const char *name)
	{
		return 0;
	}
	void addEnv(std::string name,std::string value){
		kv[name] = value;
	}
	kgl_auto_cstr parseString(const char *str);
	kgl_auto_cstr parseDirect(char *buf);
	/*
	 * 是否支持数组
	 */
	bool dimModel;
	/*
	 * 是否支持block
	 */
	bool blockModel;
	/*
	 *变量控制符
	 */
	char envChar;
	bool strictModel;
private:
	void clean();
	void controlCode(char *code);
	bool controlString(const char control_char);
	bool isControlChar(const char control_char)
	{
		if(control_char == envChar){
			return true;
		}
		if(control_char == '#' && blockModel){
			return true;
		}
		if(control_char == '@' && dimModel){
			return true;
		}
		return false;
	}
	void parseString();
	int getControlIndex(const char *value);
	char *hot;
	char *buf;
	KStringStream*dst;
	KControlCodeBlock *curBlock;
	std::list<KControlCodeBlock *> blockStack;
	std::map<std::string,std::string> kv;
};
void set_program_home_env(const char* kangle_home);
#endif
