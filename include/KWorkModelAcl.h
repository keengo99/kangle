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
	KAcl *new_instance() override {
		return new KWorkModelAcl();
	}
	const char *getName() override {
		return "work_model";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return KBIT_TEST(rq->sink->get_server_model(),flag)>0;
	}
	void get_display(KWStream& s)override {
		getFlagString(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		flag = 0;
#ifdef WORK_MODEL_TCP
		if (attribute["tcp"] == "1") {
			flag |= WORK_MODEL_TCP;
		}
#endif
		if (attribute["ssl"] == "1") {
			flag |= WORK_MODEL_SSL;
		}
		
	}
	void get_html(KWStream &s) override {
		KWorkModelAcl* m_chain = (KWorkModelAcl*)this;
#ifdef WORK_MODEL_TCP
		s << "<input type=checkbox name='tcp' value='1' ";
		if (m_chain && KBIT_TEST(m_chain->flag,WORK_MODEL_TCP)) {
			s << "checked";
		}
		s << ">tcp";
#endif
		s << "<input type=checkbox name='ssl' value='1' ";
		if (m_chain && KBIT_TEST(m_chain->flag, WORK_MODEL_SSL)) {
			s << "checked";
		}
		s << ">ssl";
	}
private:
	int flag;
	void getFlagString(KWStream& s)
	{
#ifdef WORK_MODEL_TCP
		if (KBIT_TEST(flag, WORK_MODEL_TCP)) {
			s << "tcp='1' ";
		}
#endif
		if (KBIT_TEST(flag, WORK_MODEL_SSL)) {
			s << "ssl='1' ";
		}

	}
};
#endif
