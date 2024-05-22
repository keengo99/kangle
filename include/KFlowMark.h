#ifndef KFLOWMARK_H
#define KFLOWMARK_H
#include "KFlowInfo.h"
class KFlowMark : public KMark
{
public:
	KFlowMark()
	{
		fi = NULL;
	}
	~KFlowMark()
	{
		if (fi) {
			fi->release();
		}
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (fi) {
			fi->addRef();
			rq->pushFlowInfo(fi);
			return KF_STATUS_REQ_TRUE;
		}
		return KF_STATUS_REQ_FALSE;
	}
	KMark *new_instance() override
	{
		return new KFlowMark();
	}
	const char *getName() override
	{
		return "flow";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input type=checkbox name='reset' value='1'>reset";
	}
	void get_display(KWStream& s) override {
		if (fi) {
			s << fi->flow << " " << (fi->flow>0?(fi->cache * 100) / fi->flow :0)<< "%";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if (fi==NULL) {
			fi = new KFlowInfo;
		}
		if (attribute["reset"]=="1") {
			fi->reset();
		}
	}

private:
	KFlowInfo *fi;
};
#endif
