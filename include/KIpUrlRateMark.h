#ifndef KIPURLRAGEMARK_H
#define KIPURLRAGEMARK_H
#include "KMark.h"
#include "KTargetRate.h"
#include "KIpList.h"
//{{ent
#ifdef ENABLE_BLACK_LIST
class KIpUrlRateMark : public KMark
{
public:
	KIpUrlRateMark()
	{
		lastFlushTime = 0;
		request = 0;
		second = 0;
		block_time=60;
		iplist = new KIpList;
	}
	~KIpUrlRateMark()
	{
		iplist->release();
	}
	KMark * new_instance() override {
		return new KIpUrlRateMark();
	}
	const char *get_module() const override {
		return "ip_url_rate";
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (match(rq,obj)) {
			return KF_STATUS_REQ_TRUE;
		}
		return KF_STATUS_REQ_FALSE;
	}	
	void get_display(KWStream& s) override {
		s << "&gt;" << request << "/" << second << "s ";
		s << rate.getCount();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		request = atoi(attribute["request"].c_str());
		second = atoi(attribute["second"].c_str());
		block_time = atoi(attribute["block_time"].c_str());
		if (block_time<=0) {
			block_time = 60;
		}
	}
	void get_html(KWStream& s) override {
		KIpUrlRateMark *m = (KIpUrlRateMark *)this;
		s << "rate&gt;request:<input name='request' size=4 value='";
		if(m){
			s << m->request;
		}
		s << "'>";
		s << "second:<input name='second' size=4 value='";
		if(m){
			s << m->second;
		}
		s << "'>";
		s << "block time(second):<input name='block_time' size=4 value='";
		if(m){
			s << m->block_time;
		}
		s << "'>";
	}
private:
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (kgl_current_sec - lastFlushTime > 5) {
			rate.flush(kgl_current_sec,second);
			lastFlushTime = kgl_current_sec;
		}
		KStringBuf target(2048);
		target << rq->getClientIp() << rq->sink->data.url->host << ":" << rq->sink->data.url->port << rq->sink->data.url->path;
		const char *ip_url = target.c_str();
		if (iplist->find(ip_url,block_time,true)) {
			return true;
		}
		int r = request;
		int s;
		bool result;
		result = rate.getRate(ip_url,r,s,true);
		if (!result) {
			return false;
		}
		if (second>s) {
			iplist->AddDynamic(ip_url);
			return true;
		}
		return false;
	}
	int request;
	int second;
	int block_time;
	KIpList *iplist;
	KTargetRate rate;
	time_t lastFlushTime;
};
#endif
//}}
#endif
