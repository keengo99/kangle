#ifndef KWHITELIST_ACL_H
#define KWHITELIST_ACL_H
#include "KAcl.h"
#include "KWhiteList.h"
class KWhiteListAcl : public KAcl
{
public:
	KWhiteListAcl()
	{
		host = NULL;
		flush = false;
	}
	~KWhiteListAcl()
	{
		if (host != NULL) {
			free(host);
		}
	}
	KAcl *new_instance() override  {
		return new KWhiteListAcl();
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		const char *host = this->host;
		if (host == NULL) {
			host = rq->sink->data.url->host;
		}
		return wlm.find(host, rq->getClientIp(),this->flush);
	}
	const char *getName() override {
		return "white_list";
	}
	void get_display(KWStream& s) override {
		if (host) {
			s << host;
		}
	}
	void get_html(KModel* m, KWStream& s) override {
		s << "host:<input name='host' value='";
		if (m) {
			char *host = static_cast<KWhiteListAcl *>(m)->host;
			if (host) {
				s << host;
			}
		}
		s << "'>";
		s << "<input type=checkbox name='flush' value=1 ";
		if (m && static_cast<KWhiteListAcl *>(m)->flush) {
			s << "checked";
		}
		s << ">flush";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if (host) {
			free(host);
			host = NULL;
		}
		if (!attribute["host"].empty()) {
			host = strdup(attribute["host"].c_str());
		}
		flush = attribute["flush"] == "1";
	}	
private:
	char *host;
	bool flush;
};
class KWhiteListMark : public KMark
{
public:
	KWhiteListMark()
	{
		host = NULL;
	}
	~KWhiteListMark()
	{
		if (host != NULL) {
			free(host);
		}
	}
	KMark * new_instance() override {
		return new KWhiteListMark();
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		const char *host = this->host;
		if (host == NULL) {
			host = rq->sink->data.url->host;
		}
		auto svh = kangle::get_virtual_host(rq);
		wlm.add(host, (svh ? svh->vh->name.c_str() : NULL), rq->getClientIp(), false);
		return KF_STATUS_REQ_TRUE;
	}
	const char *getName()override {
		return "white_list";
	}
	void get_display(KWStream& s) override {
		if (host) {
			s << host;
		}
	}
	void get_html(KModel* m, KWStream& s) override {
		s << "host:<input name='host' value='";
		if (m) {
			char *host = static_cast<KWhiteListMark *>(m)->host;
			if (host) {
				s << host;
			}
		}
		s << "'>";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if (host) {
			free(host);
			host = NULL;
		}
		if (!attribute["host"].empty()) {
			host = strdup(attribute["host"].c_str());
		}
	}
private:
	char *host;
};
#endif
