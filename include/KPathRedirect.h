/*
 * KPathRedirect.h
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */

#ifndef KPATHREDIRECT_H_
#define KPATHREDIRECT_H_
#include "global.h"
#include "KFileName.h"
#include "KHttpKeyValue.h"
#include "KCountable.h"
#include "KRedirect.h"
#include <string>
#include <bitset>
#include <forward_list>
#include "KXml.h"
#include "KReg.h"
enum class KConfirmFile : uint8_t
{
	Never = 0,
	Exsit = 1,
	NonExsit = 2
};
inline const char *kgl_get_confirm_file_tips(KConfirmFile confirm_file)
{
	switch (confirm_file) {
	case KConfirmFile::Never:
		return "never";
	case KConfirmFile::Exsit:
		return "exsit";
	default:
		return "non-exsit";
	}
}
struct KParamItem
{
	KString name;
	KString value;
};
class KRedirectMethods
{
public:
	KRedirectMethods()
	{
		//memset(methods, 0, sizeof(methods));
	}
	void setMethod(const char *methodstr);
	void getMethod(KWStream &s)
	{
		if (meths[0]) {
			s << "*"_CS;
			return;
		}
		for (int i = 1; i < MAX_METHOD; i++) {
			if (meths[i]) {
				if (!s.empty()) {
					s << ",";
				}
				s << KHttpKeyValue::get_method(i)->data;
			}
		}
		return;
	}
	bool matchMethod(uint8_t method)
	{
		return meths[method];
	}
private:
	std::bitset<MAX_METHOD> meths;
};
class KBaseRedirect : public KAtomCountable {
public:
	KBaseRedirect() {
		rd = NULL;
		confirm_file = KConfirmFile::Exsit;
	}
	/**
	* 调用此处rd必须先行addRef。
	*/
	KBaseRedirect(KRedirect *rd, KConfirmFile confirmFile) {
		this->rd = rd;
		this->confirm_file = confirmFile;
	}
	bool MatchConfirmFile(bool file_exsit)
	{
		switch (confirm_file) {
		case KConfirmFile::Never:
			return true;
		case KConfirmFile::Exsit:
			return file_exsit;
		default:
			return !file_exsit;
		}
	}
#ifdef ENABLE_UPSTREAM_PARAM
	void buildParams(KWStream &s)
	{
		for (auto it = params.begin(); it != params.end(); ++it) {
			if (it != params.begin()) {
				s << " ";
			}
			s << (*it).name << "=\"" << (*it).value << "\"";
		}
	}
	void parseParams(const KString &s)
	{
		char *buf = xstrdup(s.c_str());
		parseParams(buf);
		free(buf);
	}
	void parseParams(char *buf)
	{
		params.clear();
		while (*buf) {
			while (*buf && isspace((unsigned char)*buf))
				buf++;
			char *p = strchr(buf, '=');
			if (p == NULL)
				return;
			int name_len = (int)(p - buf);
			for (int i = name_len - 1; i >= 0; i--) {
				if (isspace((unsigned char)buf[i]))
					buf[i] = 0;
				else
					break;
			}
			*p = 0;
			p++;
			char *name = buf;
			buf = p;
			buf = getString(buf, &p, NULL, true);
			if (buf == NULL) {
				return;
			}
			char *value = buf;
			buf = p;
			KParamItem item;
			item.name = name;
			item.value = value;
			params.push_front(item);
		}
	}
	std::forward_list<KParamItem> params;
#endif
	KRedirectMethods allowMethod;
	KConfirmFile confirm_file;
	KRedirect *rd;
protected:
	~KBaseRedirect() {
		if (rd) {
			rd->release();
		}
	}
};
class KPathRedirect : public KBaseRedirect {
public:
	KPathRedirect(const char *path, KRedirect *rd);
	bool match(const char *path, int len);
	char *path;
	int path_len;
protected:
	~KPathRedirect();
private:
	KReg *reg;
};

#endif /* KPATHREDIRECT_H_ */
