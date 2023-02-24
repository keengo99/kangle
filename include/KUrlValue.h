#ifndef KURLVALUE_H_
#define KURLVALUE_H_
#include<map>
#include<string>
#include "KXmlAttribute.h"
class KUrlValue
{
public:
	KUrlValue();
	virtual ~KUrlValue();
	KUrlValue *getSub(const std::string &name,int index) const;
	KUrlValue *getNextSub(const std::string &name,int &index) const;
	const std::string &get(const std::string &name) const;
	/*
	得到一个值，不会返回NULL,不存在则返回""
	*/
	const std::string& get(const char *name) const;
	/*
	得到一个值，不存在返回NULL
	*/
	const char *getx(const char *name) const;
	bool get(const std::string name,std::string &value) const;
	void get(std::map<std::string,std::string> &values) const;
	const KXmlAttribute &get() const
	{
		return attribute;
	}
	bool add(const std::string &name,const std::string &value);
	void put(const std::string& name, const std::string& value);
	void add(KUrlValue *subform);
	bool parse(const char *param);
	KXmlAttribute attribute;
	const std::string &operator[](const std::string &name) const
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
