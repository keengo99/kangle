#ifndef KPORTMAPMARK_H
#define KPORTMAPMARK_H
#include "KMark.h"
#include "KLineFile.h"
#include "KHttpRequest.h"
#include "KTcpFetchObject.h"
#ifdef WORK_MODEL_TCP
struct KPortMapItem
{
	std::string host;
	int port;
	std::string param;
};
class KPortMapMark : public KMark
{
public:
	KPortMapMark()
	{
		port = 0;
	}
	virtual ~KPortMapMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		if (!KBIT_TEST(rq->sink->get_server_model(),WORK_MODEL_TCP) || rq->is_source_empty()) {
			return false;
		}
		//rq->append_source(new KTcpFetchObject(false));
		free(rq->sink->data.url->host);
		free(rq->sink->data.raw_url->host);
		if (rq->sink->data.url->param) {
			free(rq->sink->data.url->param);
			rq->sink->data.url->param = NULL;
		}
		if (!param.empty()) {
			rq->sink->data.url->param = strdup(param.c_str());
		}
		rq->sink->data.url->host = strdup(host.c_str());
		if (port==0) {
			rq->sink->data.url->port = rq->sink->get_self_port();
		} else {
			rq->sink->data.url->port = port;
		}
		rq->sink->data.raw_url->host = strdup(rq->sink->data.url->host);
		rq->sink->data.raw_url->port = rq->sink->data.url->port;
		*fo = new KTcpFetchObject(false);
		return true;
	}
	KMark * new_instance()override
	{
		return new KPortMapMark;
	}
	const char *getName()override
	{
		return "port_map";
	}
	std::string getHtml(KModel *model)override
	{
		std::stringstream s;
		s << "host:<input name='host' value='";
		KPortMapMark *m = (KPortMapMark *)model;
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
		return s.str();
	}
	std::string getDisplay()override
	{
		std::stringstream s;
		s << host << ":" << port;
		if (!param.empty()) {
			s << "?" << param;
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)override
			
	{
		host = attribute["host"];
		port = atoi(attribute["port"].c_str());
		param = attribute["param"];
	}
	void buildXML(std::stringstream &s)override
	{
		s << "host='" << host << "' port='" << port << "' param='" << param << "'>";
	}
private:
	std::string host;
	int port;
	std::string param;
};
#endif
#endif
