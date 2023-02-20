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
	bool supportRuntime() override
	{
		return true;
	}
	KAcl *newInstance() override{
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
	std::string getDisplay() override{
		std::stringstream s;
		s << "&gt;" << request << "/" << second << "s ";
		s << rate.getCount();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override{
		request = atoi(attribute["request"].c_str());
		second = atoi(attribute["second"].c_str());
	}
	void buildXML(std::stringstream &s) override{
		s << "request='" << request << "' second='" << second << "'>";
	}
	std::string getHtml(KModel *model) override{
		std::stringstream s;
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
		return s.str();
	}
private:
	int request;
	int second;
	KTargetRate rate;
	time_t lastFlushTime;
};
#endif
