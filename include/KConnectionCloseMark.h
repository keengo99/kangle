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
	KMark *newInstance() {
		return new KConnectionCloseMark();
	}
	const char *getName() {
		return "connection_close";
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		KBIT_SET(rq->flags,RQ_CONNECTION_CLOSE);
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
	}
	void buildXML(std::stringstream &s) {
		s << ">";
	}
	std::string getHtml(KModel *model) {
		return "";
	}
private:
};
#endif
