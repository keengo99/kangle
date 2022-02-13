#ifndef KHTTPFILTERDSOMANAGE_H
#define KHTTPFILTERDSOMANAGE_H
#include "utils.h"
#include <map>
#ifdef ENABLE_KSAPI_FILTER
#if 0
class KHttpFilterDso;
class KHttpFilterDsoManage
{
public:
	KHttpFilterDsoManage();
	~KHttpFilterDsoManage();
	bool add(KHttpFilterDso *dso);
	bool add(std::map<std::string,std::string> &attribute);
	KHttpFilterDso *find(const char *name);
	void html(std::stringstream &s);
	static int cur_dso_index;
private:
	KHttpFilterDso *internal_find(const char *name);
	KHttpFilterDso *head;
	KHttpFilterDso *last;
	std::map<const char *,KHttpFilterDso *,lessp> dsos;
	KMutex lock;
};
#endif
#endif
#endif
