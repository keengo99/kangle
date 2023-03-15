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
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override;
	KMark *new_instance()override;
	const char *getName()override;
	void get_html(KModel* model, KWStream& s) override;
	void get_display(KWStream& s) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	void getValList(KWStream &s);
	KDomainMap vhc;
};
#endif

