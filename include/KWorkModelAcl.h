#ifndef KWORKMODELACL_H
#define KWORKMODELACL_H
#include "KAcl.h"
class KWorkModelAcl : public KAcl
{
public:
	KWorkModelAcl()
	{
		flag = 0;
	}
	~KWorkModelAcl()
	{
	}
	KAcl *newInstance() {
		return new KWorkModelAcl();
	}
	const char *getName() {
		return "work_model";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return TEST(rq->sink->GetBindServer()->flags,flag)>0;
	}
	std::string getDisplay() {
		std::stringstream s;
		getFlagString(s);
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		flag = 0;
//{{ent
#ifdef WORK_MODEL_TCP
		if (attribute["tcp"] == "1") {
			flag |= WORK_MODEL_TCP;
		}
#endif//}}
		if (attribute["ssl"] == "1") {
			flag |= WORK_MODEL_SSL;
		}
		
	}
	void buildXML(std::stringstream &s) {
		getFlagString(s);
		s << ">";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KWorkModelAcl *m_chain = (KWorkModelAcl *)model;
		//{{ent
#ifdef WORK_MODEL_TCP
		s << "<input type=checkbox name='tcp' value='1' ";
		if (m_chain && TEST(m_chain->flag,WORK_MODEL_TCP)) {
			s << "checked";
		}
		s << ">tcp";
#endif
		//}}
		s << "<input type=checkbox name='ssl' value='1' ";
		if (m_chain && TEST(m_chain->flag, WORK_MODEL_SSL)) {
			s << "checked";
		}
		s << ">ssl";
		return s.str();
	}
private:
	int flag;
	void getFlagString(std::stringstream &s)
	{
		//{{ent
#ifdef WORK_MODEL_TCP
		if (TEST(flag, WORK_MODEL_TCP)) {
			s << "tcp='1' ";
		}
#endif
		//}}
		if (TEST(flag, WORK_MODEL_SSL)) {
			s << "ssl='1' ";
		}

	}
};
#endif
