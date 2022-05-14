#ifndef KTIMEOUTMARK_H
#define KTIMEOUTMARK_H
#include "KMark.h"
class KTimeoutMark : public KMark
{
public:
	KTimeoutMark()
	{
		v = 0;
	};
	~KTimeoutMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		rq->sink->set_time_out(v);
		return true;
	}
	KMark *newInstance()
	{
		return new KTimeoutMark;
	}
	const char *getName()
	{
		return "timeout";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "timeout radio(0-255):<input name='v' value='";
		KTimeoutMark *m = (KTimeoutMark *)model;
		if (m) {
			s << (int)m->v;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << (int)v;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		v = atoi(attribute["v"].c_str());
	}
	void buildXML(std::stringstream &s)
	{
		s << " v='" << (int)v << "'>";
	}
private:
	unsigned char v;
};
#endif
