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
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType)
	{
		if (!KBIT_TEST(rq->sink->GetBindServer()->flags,WORK_MODEL_TCP) || rq->IsFetchObjectEmpty()) {
			return false;
		}
		rq->AppendFetchObject(new KTcpFetchObject(false));
		free(rq->url->host);
		free(rq->raw_url.host);
		if (rq->url->param) {
			free(rq->url->param);
			rq->url->param = NULL;
		}
		if (!param.empty()) {
			rq->url->param = strdup(param.c_str());
		}
		rq->url->host = strdup(host.c_str());
		if (port==0) {
			rq->url->port = rq->sink->GetSelfPort();
		} else {
			rq->url->port = port;
		}
		rq->raw_url.host = strdup(rq->url->host);
		rq->raw_url.port = rq->url->port;
		jumpType = JUMP_ALLOW;
		return true;
	}
	KMark *newInstance()
	{
		return new KPortMapMark;
	}
	const char *getName()
	{
		return "port_map";
	}
	std::string getHtml(KModel *model)
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
	std::string getDisplay()
	{
		std::stringstream s;
		s << host << ":" << port;
		if (!param.empty()) {
			s << "?" << param;
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			
	{
		host = attribute["host"];
		port = atoi(attribute["port"].c_str());
		param = attribute["param"];
	}
	void buildXML(std::stringstream &s)
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
