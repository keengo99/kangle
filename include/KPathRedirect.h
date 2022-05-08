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
#include "KXml.h"
#include "KReg.h"
#define KGL_CONFIRM_FILE_NEVER 0
#define KGL_CONFIRM_FILE_EXSIT 1
#define KGL_CONFIRM_FILE_NON_EXSIT 2

inline const char *kgl_get_confirm_file_tips(uint8_t confirm_file)
{
	switch (confirm_file) {
	case KGL_CONFIRM_FILE_NEVER:
		return "never";
	case KGL_CONFIRM_FILE_EXSIT:
		return "exsit";
	default:
		return "non-exsit";
	}
}
struct KParamItem
{
	std::string name;
	std::string value;
};
class KRedirectMethods
{
public:
	KRedirectMethods()
	{
		memset(methods, 0, sizeof(methods));
	}
	void setMethod(const char *methodstr);
	std::string getMethod()
	{
		if (methods[0]) {
			return "*";
		}
		std::stringstream s;
		for (int i = 1; i < MAX_METHOD; i++) {
			if (methods[i]) {
				if (!s.str().empty()) {
					s << ",";
				}
				s << KHttpKeyValue::getMethod(i);
			}
		}
		return s.str();
	}
	bool matchMethod(uint8_t method)
	{
		return methods[method];
	}
private:
	bool methods[MAX_METHOD];
};
class KBaseRedirect : public KAtomCountable {
public:
	KBaseRedirect() {
		rd = NULL;
		inherited = false;
		confirmFile = KGL_CONFIRM_FILE_EXSIT;
	}
	/**
	* 调用此处rd必须先行addRef。
	*/
	KBaseRedirect(KRedirect *rd, uint8_t confirmFile) {
		inherited = false;
		this->rd = rd;
		this->confirmFile = confirmFile;
	}
	bool MatchConfirmFile(bool file_exsit)
	{
		switch (confirmFile) {
		case KGL_CONFIRM_FILE_NEVER:
			return true;
		case KGL_CONFIRM_FILE_EXSIT:
			return file_exsit;
		default:
			return !file_exsit;
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " extend='";
		if (rd) {
			const char *rd_type = rd->getType();
			if (strcmp(rd_type, "mserver") == 0) {
				rd_type = "server";
			}
			s << rd_type << ":" << rd->name;
		} else {
			s << "default";
		}
		s << "'";
		s << " confirm_file='" << (int)confirmFile << "'";
		s << " allow_method='" << allowMethod.getMethod() << "'";
		//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
		if (params.size() > 0) {
			std::stringstream p;
			buildParams(p);
			s << " params='" << KXml::param(p.str().c_str()) << "'";
		}
#endif
		//}}
		s << "/>\n";
	}
	//{{ent
#ifdef ENABLE_UPSTREAM_PARAM
	void buildParams(std::stringstream &s)
	{
		std::list<KParamItem>::iterator it;
		for (it = params.begin(); it != params.end(); it++) {
			if (it != params.begin()) {
				s << " ";
			}
			s << (*it).name << "=\"" << (*it).value << "\"";
		}
	}
	void parseParams(std::string &s)
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
			params.push_back(item);
		}
	}
	std::list<KParamItem> params;
#endif
	//}}
	KRedirectMethods allowMethod;
	bool inherited;
	uint8_t confirmFile;
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
