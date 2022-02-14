#ifndef KURLVALUE_H_
#define KURLVALUE_H_
#include<map>
#include<string>
/* @deprecated 用KUrlParser */
class KUrlValue
{
public:
	KUrlValue();
	virtual ~KUrlValue();
	KUrlValue *getSub(std::string name,int index);
	KUrlValue *getNextSub(std::string name,int &index);
	std::string get(const std::string name);
	/*
	得到一个值，不会返回NULL,不存在则返回""
	*/
	std::string get(const char *name);
	/*
	得到一个值，不存在返回NULL
	*/
	const char *getx(const char *name);
	bool get(const std::string name,std::string &value);
	void get(std::map<std::string,std::string> &values);
	std::map<std::string,std::string> *get()
	{
		return &attribute;
	}
	bool add(std::string name,std::string value);
	void add(KUrlValue *subform);
	bool parse(const char *param);
	std::map<std::string,std::string> attribute;
	std::string operator[](std::string name)
	{
		return attribute[name];
	}
	std::map<std::string,KUrlValue *> subs;
	KUrlValue *next;
private:
	bool flag;
	KUrlValue *sub;
};

#endif /*KURLVALUE_H_*/
