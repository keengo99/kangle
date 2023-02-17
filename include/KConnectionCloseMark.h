#ifndef KCONTEXTECTIONCLOSEMARK_H
#define KCONTEXTECTIONCLOSEMARK_H
#include "KMark.h"
class KConnectionCloseMark : public KMark
{
public:
	KConnectionCloseMark()
	{
	}
	~KConnectionCloseMark()
	{
	}
	KMark *new_instance()override {
		return new KConnectionCloseMark();
	}
	const char *getName() override {
		return "connection_close";
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,  KFetchObject** fo) override
	{
		KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override {
	}
	void buildXML(std::stringstream &s) override {
		s << ">";
	}
	std::string getHtml(KModel *model)override {
		return "";
	}
private:
};
#endif
