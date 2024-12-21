#ifndef KPORTMAPMARK_H
#define KPORTMAPMARK_H
#include "KMark.h"
#include "KLineFile.h"
#include "KHttpRequest.h"
#include "KTcpFetchObject.h"
#ifdef WORK_MODEL_TCP
class KPortMapMark : public KMark
{
public:
	KPortMapMark()
	{
	}
	virtual ~KPortMapMark()
	{
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		if (!KBIT_TEST(rq->sink->get_server_model(),WORK_MODEL_TCP)) {
			return KF_STATUS_REQ_FALSE;
		}
		rq->ctx.no_http_header = 1;
		free(rq->sink->data.url->host);
		free(rq->sink->data.raw_url.host);
		if (rq->sink->data.url->param) {
			free(rq->sink->data.url->param);
			rq->sink->data.url->param = NULL;
		}
		if (!param.empty()) {
			rq->sink->data.url->param = strdup(param.c_str());
		}
		rq->sink->data.url->host = strdup(host.c_str());
		int port = atoi(this->port.c_str());
		if (port==0) {
			rq->sink->data.url->port = rq->sink->get_self_port();
		} else {
			rq->sink->data.url->port = port;
		}
		rq->sink->data.raw_url.host = strdup(rq->sink->data.url->host);
		rq->sink->data.raw_url.port = rq->sink->data.url->port;
		if (this->port.find('s')) {
			KBIT_SET(rq->sink->data.url->flags, KGL_URL_ORIG_SSL);
		}
		fo.reset(new KTcpFetchObject(false));
		return KF_STATUS_REQ_TRUE;
	}
	KMark * new_instance() override
	{
		return new KPortMapMark;
	}
	const char *getName()override
	{
		return "port_map";
	}
	void get_html(KWStream& s) override {
		s << "host:<input name='host' value='";
		KPortMapMark *m = (KPortMapMark *)this;
		if (m) {
			s << m->host;
		}
		s << "'> port:<input name='port' size=4' value='";
		if (m) {
			s << m->port;
		}
		s << "'> param:<input name='param' value='";
		if (m) {
			s << m->param;
		}
		s << "'>";
	}
	void get_display(KWStream& s)override {
		s << host << ":" << port;
		if (!param.empty()) {
			s << "?" << param;
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		host = attribute["host"];
		port = attribute["port"];
		param = attribute["param"];
	}
private:
	KString host;
	KString port;
	KString param;
};
#endif
#endif
