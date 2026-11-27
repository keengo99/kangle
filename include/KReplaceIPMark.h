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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (header.empty()) {
#ifdef ENABLE_PROXY_PROTOCOL
			auto proxy = rq->sink->get_proxy_info();
			if (proxy && proxy->src) {				
				char ips[MAXIPLEN];
				if (ksocket_sockaddr_ip(proxy->src, ips, MAXIPLEN - 1)) {
					rq->sink->set_client_ip(ips);
					return KF_STATUS_REQ_TRUE;
				}
			}
#endif
			return KF_STATUS_REQ_FALSE;
		}
		KHttpHeader *h = rq->sink->data.get_header();
		KHttpHeader *prev = NULL;
		while (h) {
			if (kgl_is_attr(h,header.c_str(),header.size())) {
				KRegSubString *sub = NULL;
				if (val) {
					sub = val->matchSubString(h->buf+h->val_offset, h->val_len, 0);
				}
				char ip[MAXIPLEN];
				bool ip_ok = false;
				if (val == NULL) {
					ip_ok = parse_ip(h->buf + h->val_offset, h->val_len, ip, sizeof(ip));
				} else if (sub) {
					const char* s = sub->getString(1);
					ip_ok = s && parse_ip(s, strlen(s), ip, sizeof(ip));
				}
				if (!ip_ok) {
					/* not an address: keep the connection's ip, it is
					 * also written to logs and checked by ip rules */
					delete sub;
					sub = NULL;
				}
				if (sub || (val == NULL && ip_ok)) {
					if (prev) {
						prev->next = h->next;
					} else {
						rq->sink->data.header = h->next;
					}
					if (rq->sink->data.last == h) {
						rq->sink->data.last = prev;
					}
					rq->sink->set_client_ip(ip);
					//xfree_header(h);
					if (KBIT_TEST(rq->sink->data.raw_url.flags,KGL_URL_ORIG_SSL)) {
						KBIT_SET(rq->sink->data.raw_url.flags,KGL_URL_SSL);
						if (rq->sink->data.raw_url.port == 80) {
							rq->sink->data.raw_url.port = 443;
						}
					} else {
						KBIT_CLR(rq->sink->data.raw_url.flags,KGL_URL_SSL);
						if (rq->sink->data.raw_url.port == 443) {
							rq->sink->data.raw_url.port = 80;
						}
					}
					delete sub;
					return KF_STATUS_REQ_TRUE;
				}
			}
			prev = h;
			h = h->next;
		}
		return KF_STATUS_REQ_FALSE;
	}
	KMark * new_instance()override
	{
		return new KReplaceIPMark;
	}
	const char* get_module() const override
	{
		return "replace_ip";
	}
	void get_html(KWStream& s) override {
		s << "header:<input name='header' value='";
		KReplaceIPMark *m = (KReplaceIPMark *)this;
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
	}
	void get_display(KWStream& s) override {
		s << header;
		if (val) {
			s << ":" << val->getModel();
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		header = attribute["header"];
		auto val = attribute["val"];
		if (this->val) {
			delete this->val;
			this->val = NULL;
		}
		if (!val.empty()) {
			this->val = new KReg;
			this->val->setModel(val.c_str(), KGL_PCRE_CASELESS);
		}
	}	
private:
	/* Take the address from a header value: the last entry of a list (the
	 * one the nearest proxy added), without spaces or [] around IPv6.
	 * Anything that is not a numeric address is refused. */
	static bool parse_ip(const char* s, size_t len, char* out, int out_len) {
		const char* comma = (const char*)kgl_memrchr(s, ',', len);
		if (comma) {
			len -= (comma + 1 - s);
			s = comma + 1;
		}
		while (len > 0 && (*s == ' ' || *s == '\t')) {
			s++;
			len--;
		}
		while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
			len--;
		}
		if (len > 2 && s[0] == '[' && s[len - 1] == ']') {
			s++;
			len -= 2;
		}
		char ip[MAXIPLEN];
		if (len == 0 || len >= sizeof(ip)) {
			return false;
		}
		kgl_memcpy(ip, s, len);
		ip[len] = '\0';
		sockaddr_i addr;
		if (!ksocket_getaddr(ip, 0, AF_UNSPEC, AI_NUMERICHOST, &addr)) {
			return false;
		}
		return ksocket_sockaddr_ip(&addr, out, out_len);
	}
	KString header;
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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KHttpHeader *h = rq->sink->data.remove(_KS("x-real-ip-sign"));
		if (h == NULL) {
			return KF_STATUS_REQ_FALSE;
		}
		char *sign = (char *)memchr(h->buf+h->val_offset, '|', h->val_len);
		if (sign == NULL) {
			//xfree_header(h);
			return KF_STATUS_REQ_FALSE;
		}
		*sign = '\0';		
		bool matched = false;
		KMD5_CTX context;
		unsigned char digest[17];
		char buf[33];
		KMD5Init(&context);
		KMD5Update(&context, (unsigned char *)h->buf+h->val_offset, sign - (h->buf+h->val_offset));
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
			rq->ctx.parent_signed = true;
			char* hot = h->buf+h->val_offset;
			for (;;) {
				char* p = strchr(hot, ',');
				if (p) {
					*p = '\0';
				}
				char* val = strchr(hot, '=');
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
		//xfree_header(h);
		return matched? KF_STATUS_REQ_TRUE: KF_STATUS_REQ_FALSE;
	}
	KMark * new_instance()override
	{
		return new KParentMark;
	}
	const char * get_module() const override
	{
		return "parent";
	}
	void get_html(KWStream& s) override {
		s << "sign:<input name='sign' value='";
		KParentMark *m = (KParentMark *)this;
		if (m) {
			for (int i = 0; i < 2; i++) {
				if (m->upstream_sign[i].data) {
					s.write_all(m->upstream_sign[i].data, m->upstream_sign[i].len);
					if (i == 0) {
						s << "|";
					}
				}
			}
		}
		s << "'>";
	}
	void get_display(KWStream& s) override {
		for (int i = 0; i < 2; i++) {
			if (upstream_sign[i].data) {
				s.write_all(upstream_sign[i].data, upstream_sign[i].len);
				if (i == 0) {
					s << "|";
				}
			}
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
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

