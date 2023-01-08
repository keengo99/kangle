#ifndef KDSOFILTER_H_99
#define KDSOFILTER_H_99
#include "KHttpStream.h"
#include "ksapi.h"
class KDsoFilter : public KHttpStream
{
public:
	KDsoFilter(kgl_filter* filter, void* dso_ctx);
	~KDsoFilter();
	int32_t get_feature() override {
		return filter->flags;
	}
	void connect(KWriteStream* st, bool autoDelete) override {
		ctx.cn = st;
		KHttpStream::connect(st, autoDelete);
	}
	KGL_RESULT flush(void* rq) override;
	KGL_RESULT write_all(void* rq, const char* buf, int len)override;
	KGL_RESULT write_end(void* rq, KGL_RESULT result)override;
	KGL_RESULT sendfile(void* rq, kfiber_file* fp, int64_t* len) override;
	bool support_sendfile(void* arg) override;
	kgl_filter_context ctx;
	kgl_filter* filter;
};
#endif
