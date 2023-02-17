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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
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
	std::string getHtml(KModel *model)override
	{
		std::stringstream s;
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
		return s.str();
	}
	std::string getDisplay()override
	{
		std::stringstream s;
		s << (INT64)v;
		if (hard) {
			s << "H";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)override
	{
		v = (time_t)(string2int(attribute["v"].c_str()));
		if (v > kgl_current_sec) {
			v = kgl_current_sec;
		}
		hard = atoi(attribute["hard"].c_str());
	}
	void buildXML(std::stringstream &s)override
	{
		s << " v='" << (INT64)v << "' hard='" << hard << "'>";
	}
private:
	time_t v;
	int hard;
};
#endif
