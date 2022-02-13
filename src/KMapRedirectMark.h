#ifndef KMAPREDIRECTMARK_H
#define KMAPREDIRECTMARK_H
#include "KMark.h"
#include "KVirtualHostContainer.h"
class KMapRedirectItem {
public:
	KMapRedirectItem()
	{
		memset(this, 0, sizeof(KMapRedirectItem));
	}
	~KMapRedirectItem()
	{
		if (rewrite != NULL) {
			free(rewrite);
		}
	}
	char *rewrite;
	int code;
};
class KMapRedirectMark : public KMark{
public:
	KMapRedirectMark();
	virtual ~KMapRedirectMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);

	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
		;
	void buildXML(std::stringstream &s);
private:
	std::string getValList();
	KVirtualHostContainer vhc;
};
#endif

