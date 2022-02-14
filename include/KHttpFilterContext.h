#ifndef KHTTPFILTERCONTEXT_H
#define KHTTPFILTERCONTEXT_H
#include "global.h"
#include "ksapi.h"
#include <assert.h>
#include <stdlib.h>
class KHttpRequest;
#ifdef ENABLE_KSAPI_FILTER
struct KHttpFilterRequestContext
{
	void *ctx;
	DWORD disabled_flags;
};
#if 0
class KHttpFilterContext
{
public:
	KHttpFilterContext();
	~KHttpFilterContext();
	void init(KHttpRequest *rq);
	DWORD restore(int index,void *model_ctx=NULL)
	{
		ctx.pFilterContext = filterContext[index].ctx;
		ctx.ulReserved = index;
		ctx.pModelContext = model_ctx;
		return filterContext[index].disabled_flags;
	}
	void save(int index)
	{
		assert(index == (int)ctx.ulReserved);
		filterContext[index].ctx = ctx.pFilterContext;
		ctx.pModelContext = NULL;
	}
	kgl_filter_context ctx;
	KHttpFilterRequestContext *filterContext;
};
#endif
#endif
#endif
