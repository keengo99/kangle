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
	bool supportRuntime()
	{
		return true;
	}
	KMark *newInstance() {
		return new KIpUrlRateMark();
	}
	const char *getName() {
		return "ip_url_rate";
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (match(rq,obj)) {
			jumpType = JUMP_DROP;
			return true;
		}
		return false;
	}	
	std::string getDisplay() {
		std::stringstream s;
		s << "&gt;" << request << "/" << second << "s ";
		s << rate.getCount();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		request = atoi(attribute["request"].c_str());
		second = atoi(attribute["second"].c_str());
		block_time = atoi(attribute["block_time"].c_str());
		if (block_time<=0) {
			block_time = 60;
		}
	}
	void buildXML(std::stringstream &s) {
		s << "request='" << request << "' second='" << second << "' block_time='" << block_time << "'>";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KIpUrlRateMark *m = (KIpUrlRateMark *)model;
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
		return s.str();
	}
private:
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (kgl_current_sec - lastFlushTime > 5) {
			rate.flush(kgl_current_sec,second);
			lastFlushTime = kgl_current_sec;
		}
		KStringBuf target(2048);
		target << rq->getClientIp() << rq->url->host << ":" << rq->url->port << rq->url->path;
		char *ip_url = target.getString();
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
