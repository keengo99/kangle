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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		if (fi) {
			fi->addRef();
			rq->pushFlowInfo(fi);
			return true;
		}
		return false;
	}
	KMark *new_instance() override
	{
		return new KFlowMark();
	}
	const char *getName() override
	{
		return "flow";
	}
	std::string getHtml(KModel *model) override
	{
		std::stringstream s;
		s << "<input type=checkbox name='reset' value='1'>reset";
		return s.str();
	}
	std::string getDisplay() override
	{
		std::stringstream s;
		if (fi) {
			s << fi->flow << " " << (fi->flow>0?(fi->cache * 100) / fi->flow :0)<< "%";
		}
		return s.str();
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
