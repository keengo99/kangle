#ifndef KIPSPEEDLIMITMARK_H
#define KIPSPEEDLIMITMARK_H
#include <map>
#include "KMark.h"
#include "KSpeedLimit.h"
#include "utils.h"
class KIpSpeedLimitMark;
struct KIpSpeedLimitContext
{
	char *ip;
	KIpSpeedLimitMark *mark;
};
void ip_speed_limit_clean(void *data);
class KIpSpeedLimitMark : public KMark
{
public:
	KIpSpeedLimitMark()
	{
		speed_limit = 0;
	}
	virtual ~KIpSpeedLimitMark()
	{
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		for (it=ips.begin();it!=ips.end();it++) {
			(*it).second->release();
			free((*it).first);
		}
	}
	void requestClean(char *ip)
	{
		lock.Lock();
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		it = ips.find(ip);
		if (it!=ips.end() && (*it).second->getRef()<=1) {
			free((*it).first);
			(*it).second->release();
			ips.erase(it);
		}
		lock.Unlock();	
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		const char *ip = rq->getClientIp();
		lock.Lock();
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		it = ips.find((char *)ip);
		KSpeedLimit *sl = NULL;
		if (it==ips.end()) {
			sl = new KSpeedLimit;
			sl->setSpeedLimit(speed_limit);
			ips.insert(std::pair<char *,KSpeedLimit *>(strdup(ip),sl));
		} else {
			sl = (*it).second;
		}
		sl->addRef();
		lock.Unlock();
		rq->pushSpeedLimit(sl);
		KIpSpeedLimitContext *speed_limit_context = new KIpSpeedLimitContext();
		speed_limit_context->ip = strdup(ip);
		speed_limit_context->mark = this;
		add_ref();
		rq->registerRequestCleanHook(ip_speed_limit_clean,speed_limit_context);
		return KF_STATUS_REQ_TRUE;
	}
	KMark *new_instance()override
	{
		return new KIpSpeedLimitMark;
	}
	const char *getName()override
	{
		return "ip_speed_limit";
	}
	void get_html(KModel* model, KWStream& s) override {
		KIpSpeedLimitMark *m = (KIpSpeedLimitMark *)model;
		s << "speed_limit:<input name='speed_limit' value='";
		if (m) {
			s << get_size(m->speed_limit);
		}
		s << "'> / second";
	}
	void get_display(KWStream& s) override {
		s << get_size(speed_limit) << "/second ,record:";
		lock.Lock();
		s << (int64_t)ips.size();
		lock.Unlock();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		speed_limit = (int)get_size(xml->attr()("speed_limit"));
	}	
private:
	int speed_limit;
	std::map<char *,KSpeedLimit *,lessp> ips;
	KMutex lock;
};
#endif
