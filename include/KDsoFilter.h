#ifndef KDSOFILTER_H_99
#define KDSOFILTER_H_99
#include "KHttpStream.h"
#include "ksapi.h"
class KDsoFilter : public KHttpStream
{
public:
	KDsoFilter(kgl_filter *filter,void *dso_ctx);
	~KDsoFilter();
	KGL_RESULT flush(void *rq) override;
	KGL_RESULT write_all(void*rq, const char *buf, int len)override;
	KGL_RESULT write_end(void*rq, KGL_RESULT result)override;
	kgl_filter_context ctx;
	kgl_filter *filter;
};
#endif
