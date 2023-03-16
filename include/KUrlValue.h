#ifndef KURLVALUE_H_
#define KURLVALUE_H_
#include<map>
#include<string>
#include "KXmlAttribute.h"
#include "KXmlDocument.h"
#include "KConfigTree.h"
class KUrlValue
{
public:
	KUrlValue();
	virtual ~KUrlValue();
	KUrlValue *getSub(const KString&name,int index) const;
	KUrlValue *getNextSub(const KString&name,int &index) const;
	const KString &get(const KString&name) const;
	/*
	得到一个值，不会返回NULL,不存在则返回""
	*/
	const KString& get(const char *name) const;
	/*
	得到一个值，不存在返回NULL
	*/
	const char *getx(const char *name) const;
	bool get(const KString name, KString&value) const;
	void get(std::map<KString, KString> &values) const;
	const KXmlAttribute &get() const
	{
		return attribute;
	}
	bool add(const KString&name,const KString&value);
	void put(const KString& name, const KString& value);
	void add(KUrlValue *subform);
	bool parse(const char *param);
	bool parse(char* param);
	KXmlAttribute attribute;
	khttpd::KSafeXmlNode to_xml(const char *tag,size_t len,const char *vary=nullptr,size_t vary_len=0) {
		auto xml = kconfig::new_xml(tag, len,vary,vary_len);
		xml->attributes().swap(attribute);
		return xml;
	}
	const KString &operator[](const KString&name) const
	{
		return attribute[name];
	}
	std::map<KString,KUrlValue *> subs;
	KUrlValue *next;
private:
	bool flag;
	KUrlValue *sub;
};

#endif /*KURLVALUE_H_*/
