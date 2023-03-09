#ifndef KSTUBSTATUSMARK_H_
#define KSTUBSTATUSMARK_H_

#include "KMark.h"
#include "KBufferFetchObject.h"

#ifdef ENABLE_STAT_STUB
class KStubStatusMark: public KMark {
public:
	KStubStatusMark()
	{
	}
	~KStubStatusMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		rq->response_status(STATUS_OK);
		rq->response_header(kgl_expand_string("Content-Type"), kgl_expand_string("text/plain"));
		rq->response_header(kgl_expand_string("Cache-Control"), kgl_expand_string("no-cache,no-store"));
		rq->response_header(kgl_expand_string("Server"),conf.serverName,conf.serverNameLength);
		KAutoBuffer s(rq->sink->pool);
		s << "Active connections: " << (int)total_connect << " \n";
		s << "server accepts handled requests\n";
		s << " " <<(INT64)katom_get64((void *)&kgl_total_servers) << " " << (INT64)katom_get64((void *)&kgl_total_accepts) << " " << (INT64)katom_get64((void *)&kgl_total_requests) << " \n";
		s << "Reading: " << (int)katom_get((void *)&kgl_reading) << " Writing: " << (int)katom_get((void *)&kgl_writing) << " Waiting: " << (int)katom_get((void *)&kgl_waiting) << " \n";
		*fo = new KBufferFetchObject(s.getHead(), s.getLen());
		//jump_type = JUMP_DROP;
		return true;
	}
	KMark * new_instance() override
	{
		return new KStubStatusMark();
	}
	const char *getName() override
	{
		return "stub_status";
	}
	std::string getHtml(KModel *model) override
	{
		return "";
	}
	std::string getDisplay() override
	{
		return "";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override
	{
	}
private:
};
#endif
#endif
