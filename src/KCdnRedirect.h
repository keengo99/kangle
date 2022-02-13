#ifndef KCDNREDIRECT_H
#define KCDNREDIRECT_H
#include "KRedirect.h"
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
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	const char *getType()
	{
		return "cdn";
	}
};
#endif
