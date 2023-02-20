#ifndef KIPURLRAGEACL_H
#define KIPURLRAGEACL_H
#include "KAcl.h"
#include "KTargetRate.h"
//{{ent
#ifdef ENABLE_BLACK_LIST
class KIpUrlRateAcl : public KAcl
{
public:
	KIpUrlRateAcl()
	{
		lastFlushTime = 0;
		request = 0;
		second = 0;
	}
	~KIpUrlRateAcl()
	{
	}
	bool supportRuntime() override
	{
		return true;
	}
	KAcl *new_instance() override {
		return new KIpUrlRateAcl();
	}
	const char *getName() override {
		return "ip_url_rate";
	}	
	std::string getDisplay() override  {
		std::stringstream s;
		s << "&gt;" << request << "/" << second << "s ";
		s << rate.getCount();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override
	{
		request = atoi(attribute["request"].c_str());
		second = atoi(attribute["second"].c_str());
	}
	void buildXML(std::stringstream &s) override {
		s << "request='" << request << "' second='" << second << "'>";
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		KIpUrlRateAcl *m = (KIpUrlRateAcl *)model;
		s << "rate&gt;request:<input name='request' size=4 value='";
		if (m) {
			s << m->request;
		}
		s << "'>";
		s << "second:<input name='second' size=4 value='";
		if (m) {
			s << m->second;
		}		
		s << "'>";
		return s.str();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		if (kgl_current_sec - lastFlushTime > 5) {
			rate.flush(kgl_current_sec,second);
			lastFlushTime = kgl_current_sec;
		}
		KStringBuf target(2048);
		target << rq->getClientIp() << rq->sink->data.url->host << ":" << rq->sink->data.url->port << rq->sink->data.url->path;
		char *ip_url = target.getString();
		int r = request;
		int s;
		bool result = rate.getRate(ip_url,r,s,true);
		if (!result) {
			return false;
		}
		if (second>s) {
			return true;
		}
		return false;
	}
private:
	int request;
	int second;
	KTargetRate rate;
	time_t lastFlushTime;
};
#endif
//}}
#endif
