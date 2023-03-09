#ifndef KDSOREDIRECT_H
#define KDSOREDIRECT_H
#include "KRedirect.h"
#include "ksapi.h"


class KDsoRedirect : public KRedirect
{
public:
	KDsoRedirect(const char *dso_name, kgl_upstream*us)
	{
		name = dso_name;
		name += ":";
		name += us->name;
		this->us = us;
	}
	~KDsoRedirect()
	{
	
	}
	kev_result connect(KHttpRequest *rq, KAsyncFetchObject *fo)
	{
		kassert(false);
		return kev_err;
	}
	const char *getType() override {
		return "dso";
	}

	KRedirectSource *makeFetchObject(KHttpRequest *rq, KFileName *file) override;
	KRedirectSource *makeFetchObject(KHttpRequest *rq, void *model_ctx);
	friend class KDsoExtend;
	friend class KDsoAsyncFetchObject;
private:
	kgl_upstream *us;
};
#endif
