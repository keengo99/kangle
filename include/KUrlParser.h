#ifndef KURLPARSER_H
#define KURLPARSER_H
#include <map>
#include <string.h>
#include "kforwin32.h"
#include "KUrl.h"
#include "kmalloc.h"
struct lessp_url_name {
	bool operator()(const char * __x, const char * __y) const {
		return strcasecmp(__x, __y) < 0;
	}
};
class KParamValue
{
public:
	KParamValue(const char *val)
	{
		this->val = val;
		next = NULL;
		last = this;
	}
	const char *val;
	KParamValue *next;
	KParamValue *last;
};
class KUrlParser
{
public:
	KUrlParser()
	{
		buf = NULL;
	}
	~KUrlParser()
	{
		if (buf) {
			xfree(buf);
		}
		std::map<const char *,KParamValue *,lessp_url_name>::iterator it;
		for (it=m.begin();it!=m.end();it++) {
			delete (*it).second;
		}
	}
	bool parse(const char *param);
	KParamValue *getAll(const char *name)
	{
		std::map<const char *,KParamValue *,lessp_url_name>::iterator it;
		it = m.find(name);
		if (it==m.end()) {
			return NULL;
		}
		return (*it).second;
	}
	const char *get(const char *name)
	{
		KParamValue *pv = getAll(name);
		if (pv==NULL) {
			return NULL;
		}
		return pv->val;
	}
private:
	void add(const char *name,const char *value)
	{
		std::map<const char *,KParamValue *,lessp_url_name>::iterator it;
		it = m.find(name);
		KParamValue *pv = new KParamValue(value);
		if (it==m.end()) {
			m.insert(std::pair<const char *,KParamValue *>(name,pv));
		} else {
			KParamValue *pv_head = (*it).second;
			assert(pv_head->last);
			pv_head->last->next = pv;
			pv_head->last = pv;
		}
	}
	char *buf;
	std::map<const char *,KParamValue *,lessp_url_name> m;
};
int url_decode(char *url_msg, int url_len = 0,KUrl *url=NULL,bool space2plus=true);
#endif
