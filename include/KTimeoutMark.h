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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		rq->sink->set_time_out(v);
		return true;
	}
	KMark * new_instance()override
	{
		return new KTimeoutMark;
	}
	const char *getName()override
	{
		return "timeout";
	}
	std::string getHtml(KModel *model)override
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
	std::string getDisplay()override
	{
		std::stringstream s;
		s << (int)v;
		return s.str();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		v = atoi(attribute["v"].c_str());
	}
private:
	unsigned char v;
};
#endif
