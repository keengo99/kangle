#ifndef KREPLACEIPMARK_H
#define KREPLACEIPMARK_H
#include "KMark.h"
#include "kmd5.h"

class KReplaceIPMark : public KMark
{
public:
	KReplaceIPMark()
	{
		header = "X-Real-Ip";
		val = NULL;
	}
	~KReplaceIPMark()
	{
		if (val) {
			delete val;
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		if (header.empty()) {
#ifdef ENABLE_PROXY_PROTOCOL
			kconnection *c = rq->sink->GetConnection();
			if (c && c->proxy && c->proxy->src) {				
				char ips[MAXIPLEN];
				if (ksocket_sockaddr_ip(c->proxy->src, ips, MAXIPLEN - 1)) {
					rq->sink->set_client_ip(ips);
					return true;
				}
			}
#endif
			return false;
		}
		KHttpHeader *h = rq->sink->data.GetHeader();
		KHttpHeader *prev = NULL;
		while (h) {
			if (strcasecmp(h->attr, header.c_str()) == 0) {
				KRegSubString *sub = NULL;
				if (val) {
					sub = val->matchSubString(h->val, strlen(h->val), 0);
				}
				if (val == NULL || sub) {
					if (prev) {
						prev->next = h->next;
					} else {
						rq->sink->data.header = h->next;
					}
					if (rq->sink->data.client_ip) {
						free(rq->sink->data.client_ip);
						rq->sink->data.client_ip = NULL;
					}
					if (val == NULL) {
						rq->sink->data.client_ip = h->val;
					} else {
						free(h->val);
						char *ip = sub->getString(1);
						if (ip) {
							rq->sink->data.client_ip = strdup(ip);
						}
					}
					free(h->attr);
					free(h);
					if (KBIT_TEST(rq->sink->data.raw_url->flags,KGL_URL_ORIG_SSL)) {
						KBIT_SET(rq->sink->data.raw_url->flags,KGL_URL_SSL);
						if (rq->sink->data.raw_url->port == 80) {
							rq->sink->data.raw_url->port = 443;
						}
					} else {
						KBIT_CLR(rq->sink->data.raw_url->flags,KGL_URL_SSL);
						if (rq->sink->data.raw_url->port == 443) {
							rq->sink->data.raw_url->port = 80;
						}
					}
					return true;
				}
			}
			prev = h;
			h = h->next;
		}
		return false;
	}
	KMark *newInstance()
	{
		return new KReplaceIPMark;
	}
	const char *getName()
	{
		return "replace_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "header:<input name='header' value='";
		KReplaceIPMark *m = (KReplaceIPMark *)model;
		if (m) {
			s << m->header;
		} else {
			s << "X-Real-Ip";
		}
		s << "'>";
		s << "val(regex):<input name='val' value='";
		if (m && m->val) {
			s << m->val->getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << header;
		if (val) {
			s << ":" << val->getModel();
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		header = attribute["header"];
		std::string val = attribute["val"];
		if (this->val) {
			delete this->val;
			this->val = NULL;
		}
		if (!val.empty()) {
			this->val = new KReg;
			this->val->setModel(val.c_str(), PCRE_CASELESS);
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " header='" << header << "' ";
		if (val) {
			s << "val='" << KXml::param(val->getModel()) << "' ";
		}
		s << "> ";
	}
private:
	std::string header;
	KReg *val;
};
class KParentMark : public KMark
{
public:
	KParentMark()
	{
		memset(&upstream_sign, 0, sizeof(upstream_sign));
	}
	~KParentMark()
	{
		for (int i = 0; i < 2; i++) {
			if (upstream_sign[i].data) {
				free(upstream_sign[i].data);
			}
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		KHttpHeader *h = rq->sink->data.RemoveHeader("x-real-ip-sign");
		if (h == NULL) {
			return false;
		}
		char *sign = strchr(h->val, '|');
		if (sign == NULL) {
			free(h->attr);
			free(h->val);
			free(h);
			return false;
		}
		*sign = '\0';		
		bool matched = false;
		KMD5_CTX context;
		unsigned char digest[17];
		char buf[33];
		KMD5Init(&context);
		KMD5Update(&context, (unsigned char *)h->val, sign - h->val);
		for (int i = 0; i < 2; i++) {
			if (upstream_sign[i].data == NULL) {
				continue;
			}
			KMD5_CTX ctx2 = context;
			KMD5Update(&ctx2, (unsigned char *)upstream_sign[i].data, upstream_sign[i].len);
			KMD5Final(digest, &ctx2);
			make_digest(buf, digest);
			if (strcmp(buf, sign + 1) == 0) {
				matched = true;
				break;
			}
		}
		if (matched) {
			rq->ctx->parent_signed = true;
			char *hot = h->val;
			for (;;) {
				char *p = strchr(hot, ',');
				if (p) {
					*p = '\0';
				}
				char *val = strchr(hot, '=');
				if (val) {
					*val = '\0';
					val++;
					if (strcmp(hot, "ip") == 0) {
						rq->sink->set_client_ip(val);
					} else if (strcmp(hot, "p") == 0) {
						rq->SetSelfPort(0, strcmp(val, "https") == 0);
					} else if (strcmp(hot, "sp") == 0) {
						rq->SetSelfPort(uint16_t(atoi(val)), strchr(val, 's') != NULL);
					}
				}
				if (p == NULL) {
					break;
				}
				hot = p + 1;
			}
		}
		free(h->attr);
		free(h->val);
		free(h);
		return matched;
	}
	KMark *newInstance()
	{
		return new KParentMark;
	}
	const char *getName()
	{
		return "parent";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "sign:<input name='sign' value='";
		KParentMark *m = (KParentMark *)model;
		if (m) {
			for (int i = 0; i < 2; i++) {
				if (m->upstream_sign[i].data) {
					s.write(m->upstream_sign[i].data, m->upstream_sign[i].len);
					if (i == 0) {
						s << "|";
					}
				}
			}
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		for (int i = 0; i < 2; i++) {
			if (upstream_sign[i].data) {
				s.write(upstream_sign[i].data, upstream_sign[i].len);
				if (i == 0) {
					s << "|";
				}
			}
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		
		for (int i = 0; i < 2; i++) {
			if (this->upstream_sign[i].data) {
				free(this->upstream_sign[i].data);
				this->upstream_sign[i].data = NULL;
			}
		}
		char *upstream_sign = strdup(attribute["sign"].c_str());
		char *hot = upstream_sign;
		for (int i = 0; i < 2; i++) {
			char *p = strchr(hot, '|');
			if (p) {
				*p = '\0';
			}
			this->upstream_sign[i].len = strlen(hot);
			if (this->upstream_sign[i].len > 0) {
				this->upstream_sign[i].data = strdup(hot);
			}
			if (p == NULL) {
				break;
			}
			hot = p + 1;
		}
		free(upstream_sign);
	}
	void buildXML(std::stringstream &s)
	{
		s << " sign='";
		for (int i = 0; i < 2; i++) {
			if (upstream_sign[i].data) {
				s.write(upstream_sign[i].data, upstream_sign[i].len);
				if (i == 0) {
					s << "|";
				}
			}
		}
		s << "'> ";
	}
private:
	kgl_str_t upstream_sign[2];
};
#if 0
class KSelfIPMark : public KMark
{
public:
	KSelfIPMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		if (rq->bind_ip) {
			free(rq->bind_ip);
			rq->bind_ip = NULL;
		}
		if (ip.empty()) {
			char ip[MAXIPLEN];
			rq->sink->get_self_ip(ip,sizeof(ip));
			rq->bind_ip = strdup(ip);
		} else if (ip[0] == '$') {
			rq->bind_ip = strdup(rq->getClientIp());
		} else if (ip[0] != '-') {
			rq->bind_ip = strdup(ip.c_str());
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KSelfIPMark;
	}
	const char *getName()
	{
		return "self_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "ip:<input name='ip' value='";
		KSelfIPMark *m = (KSelfIPMark *)model;
		if (m) {
			s << m->ip;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return ip;
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		ip = attribute["ip"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " ip='" << ip << "'>";
	}
private:
	std::string ip;
};
#endif
#endif

