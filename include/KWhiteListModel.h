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
	KAcl *newInstance() {
		return new KWhiteListAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		const char *host = this->host;
		if (host == NULL) {
			host = rq->sink->data.url->host;
		}
		return wlm.find(host, rq->getClientIp(),this->flush);
	}
	const char *getName() {
		return "white_list";
	}
	std::string getDisplay() {
		std::stringstream s;
		if (host) {
			s << host;
		}
		return s.str();
	}
	std::string getHtml(KModel *m) {
		std::stringstream s;
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
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
		 {
		if (host) {
			free(host);
			host = NULL;
		}
		if (!attribute["host"].empty()) {
			host = strdup(attribute["host"].c_str());
		}
		flush = attribute["flush"] == "1";
	}
	void buildXML(std::stringstream &s) {
		if (this->host) {
			s << "host='" << host << "'";
		}
		s << " flush='" << (flush ? 1 : 0) << "'";
		s << ">";
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
	KMark *newInstance() {
		return new KWhiteListMark();
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType)
	{
		const char *host = this->host;
		if (host == NULL) {
			host = rq->sink->data.url->host;
		}
		auto svh = rq->get_virtual_host();
		wlm.add(host, (svh ? svh->vh->name.c_str() : NULL), rq->getClientIp(), false);
		return true;
	}
	const char *getName() {
		return "white_list";
	}
	std::string getDisplay() {
		std::stringstream s;
		if (host) {
			s << host;
		}
		return s.str();
	}
	std::string getHtml(KModel *m) {
		std::stringstream s;
		s << "host:<input name='host' value='";
		if (m) {
			char *host = static_cast<KWhiteListMark *>(m)->host;
			if (host) {
				s << host;
			}
		}
		s << "'>";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		if (host) {
			free(host);
			host = NULL;
		}
		if (!attribute["host"].empty()) {
			host = strdup(attribute["host"].c_str());
		}
	}
	void buildXML(std::stringstream &s) {
		if (this->host) {
			s << "host='" << host << "'";
		}
		s << ">";
	}
private:
	char *host;
};
#endif
