#ifndef KIPRATEACL_H
#define KIPRATEACL_H
#include "KAcl.h"
#include "KTargetRate.h"
class KIpRateAcl : public KAcl
{
public:
	KIpRateAcl()
	{
		lastFlushTime = 0;
		request = 0;
		second = 0;
	}
	~KIpRateAcl()
	{
	}
	KAcl *new_instance() override{
		return new KIpRateAcl();
	}
	const char *getName() override{
		return "ip_rate";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		if (kgl_current_sec - lastFlushTime > 5) {
			rate.flush(kgl_current_sec,second);
			lastFlushTime = kgl_current_sec;
		}
		int r = request;
		int s;
		bool result;
		result = rate.getRate(rq->getClientIp(),r,s,true);
		if (!result) {
			return false;
		}
		return second>s;
	}
	void get_display(KWStream& s) override {
		s << "&gt;" << request << "/" << second << "s ";
		s << rate.getCount();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		request = atoi(attribute["request"].c_str());
		second = atoi(attribute["second"].c_str());
	}
	void get_html(KModel* model, KWStream& s) override {
		KIpRateAcl *m = (KIpRateAcl *)model;
		s << "rate&gt;request:<input name='request' value='";
		if(m){
			s << m->request;
		}
		s << "'>";
		s << "second:<input name='second' value='";
		if(m){
			s << m->second;
		}
		s << "'>";
	}
private:
	int request;
	int second;
	KTargetRate rate;
	time_t lastFlushTime;
};
#endif
