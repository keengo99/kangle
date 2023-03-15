#ifndef KMINOBJVERIFIEDMARK_H
#define KMINOBJVERIFIEDMARK_H
#include "time_utils.h"
class KMinObjVerifiedMark : public KMark
{
public:
	KMinObjVerifiedMark()
	{
		v = kgl_current_sec;
		hard = 0;
	};
	~KMinObjVerifiedMark()
	{
	}
	bool process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		rq->sink->data.min_obj_verified = v;
		if (hard) {
			KBIT_SET(rq->sink->data.min_obj_verified, 1);
		} else {
			KBIT_CLR(rq->sink->data.min_obj_verified, 1);
		}
		return true;
	}
	KMark * new_instance()override
	{
		return new KMinObjVerifiedMark;
	}
	const char *getName()override
	{
		return "min_obj_verified";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "min_obj_verified:<input name='v' value='";
		KMinObjVerifiedMark *m = (KMinObjVerifiedMark *)model;
		if (m) {
			s << (INT64)(m->v);
		}
		s << "'>";
		s << "<input type='checkbox' name='hard' value='1' ";
		if (m && m->hard) {
			s << "checked";
		}
		s << ">hard";
	}
	void get_display(KWStream& s) override {
		s << (INT64)v;
		if (hard) {
			s << "H";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		v = (time_t)(string2int(attribute["v"].c_str()));
		if (v > kgl_current_sec) {
			v = kgl_current_sec;
		}
		hard = atoi(attribute["hard"].c_str());
	}
private:
	time_t v;
	int hard;
};
#endif
