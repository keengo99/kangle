#ifndef KHTTP10MARK_H_
#define KHTTP10MARK_H_
#include "KMark.h"
#include "global.h"
#include "KHttpRequest.h"
class KHttp10Mark: public KMark {
public:
	KHttp10Mark()
	{
	}
	virtual ~KHttp10Mark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		rq->sink->data.http_minor = 0;
		rq->sink->data.http_major = 1;
		return true;
	}
	KMark *newInstance()
	{
		return new KHttp10Mark();
	}
	const char *getName()
	{
		return "http10";
	}
	std::string getHtml(KModel *model)
	{
		return "";
	}
	std::string getDisplay()
	{
		return "";
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
	}
	void buildXML(std::stringstream &s)
	{
		s << ">";
	}
};

#endif
