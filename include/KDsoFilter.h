#ifndef KDSOFILTER_H_99
#define KDSOFILTER_H_99
#include "KHttpStream.h"
#include "ksapi.h"
class KDsoFilter : public KHttpStream
{
public:
	KDsoFilter(kgl_filter *filter,void *dso_ctx);
	~KDsoFilter();
	KGL_RESULT flush(KHttpRequest *rq);
	KGL_RESULT write_all(KHttpRequest *rq, const char *buf, int len);
	KGL_RESULT write_end(KHttpRequest *rq, KGL_RESULT result);
	kgl_filter_context ctx;
	kgl_filter *filter;
};
#endif
