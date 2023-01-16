#ifndef KCDNREDIRECT_H
#define KCDNREDIRECT_H
#include "KRedirect.h"
#if 0
class KCdnRedirect : public KRedirect
{
public:
	KCdnRedirect(const char *target)
	{
		if (target) {
			name = target;
		}
	}
	virtual ~KCdnRedirect(){
	}
	KRedirectSource*makeFetchObject(KHttpRequest *rq, KFileName *file) override;
	const char *getType()
	{
		return "cdn";
	}
};
#endif
#endif
