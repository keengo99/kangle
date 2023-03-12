/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KLANG_H_
#define KLANG_H_
#include<map>
#include<string>
#include "utils.h"
#include "KXmlEvent.h"
class KLocalLang
{
public:
	const char * get(const char *name)
	{
		std::map<KString,KString>::iterator it;
		it = langs.find(name);
		if(it==langs.end()){
			return NULL;
		}
		return (*it).second.c_str();
	}
	bool add(KString name,KString value)
	{
		langs[name] = value;
		return true;
	}
	friend class KLang;
private:
	std::map<KString,KString> langs;
};
class KLang : public KXmlEvent 
{
public:
	KLang(std::map<char *,KLocalLang *,lessp_icase> *langs)
	{
		this->langs = langs;
	}
	KLang();
	virtual ~KLang();
	void clear();
	const char * operator [](const char *name);
	void getAllLangName(std::vector<KString> &names);
	bool load(KString file);
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
private:
	const char *get(char *lang_name,const char *name);
	KLocalLang *getLocal(char *name);
	bool add(KString lang,KString name,KString value);
	std::map<char *,KLocalLang *,lessp_icase> *langs;
	KString curName;
};
extern KLang klang;
#endif /*KLANG_H_*/
