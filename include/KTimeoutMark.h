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
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
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
	void get_html(KModel* model, KWStream& s) override {
		s << "timeout radio(0-255):<input name='v' value='";
		KTimeoutMark *m = (KTimeoutMark *)model;
		if (m) {
			s << (int)m->v;
		}
		s << "'>";
	}
	void get_display(KWStream& s) override {
		s << (int)v;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		v = atoi(attribute["v"].c_str());
	}
private:
	unsigned char v;
};
#endif
