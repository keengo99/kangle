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
	bool supportRuntime()
	{
		return true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		if (fi) {
			fi->addRef();
			rq->pushFlowInfo(fi);
			return true;
		}
		return false;
	}
	KMark *newInstance()
	{
		return new KFlowMark();
	}
	const char *getName()
	{
		return "flow";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "<input type=checkbox name='reset' value='1'>reset";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (fi) {
			s << fi->flow << " " << (fi->flow>0?(fi->cache * 100) / fi->flow :0)<< "%";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		if (fi==NULL) {
			fi = new KFlowInfo;
		}
		if (attribute["reset"]=="1") {
			fi->reset();
		}
	}
	void buildXML(std::stringstream &s,int flag)
	{
		if (fi) {
			s << "flow='" << fi->flow << "' ";
			s << "cache='" << fi->cache << "' ";
			if (TEST(flag,CHAIN_RESET_FLOW)) {
				fi->reset();
			}
		}
		s << ">";
	}
private:
	KFlowInfo *fi;
};
#endif
