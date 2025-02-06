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
	const KString &get(const KString&name) const;
	/*
	得到一个值，不会返回NULL,不存在则返回""
	*/
	const KString& get(const char *name) const;
	/*
	得到一个值，不存在返回NULL
	*/
	const char *getx(const char *name) const;
	KString remove(const KString& name) {
		auto it = attribute.find(name);
		if (it == attribute.end()) {
			return "";
		}
		KString value(std::move((*it).second));
		attribute.erase(it);
		return value;
	}
	bool get(const KString name, KString&value) const;
	void get(std::map<KString, KString> &values) const;
	void build_config_base_path(KStringBuf &path, const KString &cfg_file) const;
	const KXmlAttribute &get() const
	{
		return attribute;
	}
	void put(const KString& name, const KString& value);
	bool parse(const char *param);
	bool parse(char* param, size_t len);
	KXmlAttribute attribute;
	khttpd::KSafeXmlNode to_xml(const char *tag,size_t len,const char *vary=nullptr,size_t vary_len=0) {
		auto xml = kconfig::new_xml(tag, len,vary,vary_len);
		xml->attributes().merge(attribute);
		return xml;
	}
	const KString &operator[](const KString&name) const
	{
		return attribute[name];
	}
	std::multimap<KString,KUrlValue *> subs;
};

#endif /*KURLVALUE_H_*/
