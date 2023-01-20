#ifndef KFOOTERFILTER_H
#define KFOOTERFILTER_H
#include "KHttpStream.h"
#if 0
class KFooterFilter : public KHttpStream
{
public:
	StreamState write_all(void *rq, const char *buf, int len) override
	{
		if (replace) {
			return STREAM_WRITE_SUCCESS;
		}
		if (head && !added && !footer.empty()) {
			added = true;
			if(KHttpStream::write_all(rq, footer.c_str(),footer.size())==STREAM_WRITE_FAILED) {
				return STREAM_WRITE_FAILED;
			}
		}
		return KHttpStream::write_all(rq, buf,len);
	}
	StreamState write_end(void *rq, KGL_RESULT result) override{
		if (!footer.empty() && (!head||replace)) {
			if (KHttpStream::write_all(rq, footer.c_str(),footer.size())==STREAM_WRITE_FAILED) {
				result = STREAM_WRITE_FAILED;
			}
		}
		return KHttpStream::write_end(rq, result);
	}
	std::string footer;
	bool head;
	bool added;
	bool replace;
private:
};
#endif
#endif
