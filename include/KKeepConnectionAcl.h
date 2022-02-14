#ifndef KKEEPCONNECTIONACL_H
#define KKEEPCONNECTIONACL_H
#include "KAcl.h"
//@deprecated use work_model instead
#if 0
class KKeepConnectionAcl : public KAcl
{
public:
	KKeepConnectionAcl()
	{
	}
	~KKeepConnectionAcl()
	{
	}
	KAcl *newInstance() {
		return new KKeepConnectionAcl();
	}
	const char *getName() {
		return "keep_connection";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return TEST(rq->workModel,WORK_MODEL_KA)>0;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "deprecated use work_model";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
	}
	void buildXML(std::stringstream &s) {
		s << ">";
	}
	std::string getHtml(KModel *model) {
		return "deprecated please use work_model acl ka flag";
	}
private:
};
#endif
#endif
