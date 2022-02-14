#ifndef KSTATUSCODEMARK_H
#define KSTATUSCODEMARK_H
#include "KMark.h"
class KStatusCodeMark: public KMark
{
public:
	KStatusCodeMark()
	{
		code = 0;
	}
	~KStatusCodeMark()
	{
	}
	KMark *newInstance() {
		return new KStatusCodeMark();
	}
	const char *getName() {
		return "status_code";
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (obj && obj->data) {
			obj->data->status_code = code;
		}
		return true;
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KStatusCodeMark *m = (KStatusCodeMark *)model;
		s << " <input name=code value='";
		if (m) {
			s << (int)m->code;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay() {
		std::stringstream s;
		s << code;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			 {
		code = atoi(attribute["code"].c_str());
	}
	void buildXML(std::stringstream &s) {
		s << "code='" << code << "'>";
	}
private:
	int code;
};
#endif
