#ifndef KREFERERACL_H
#define KREFERERACL_H
#include "KAcl.h"
#include "KVirtualHostContainer.h"
inline void referer_domain_iterator(void* arg, const char* domain, void* vh)
{
	KWStream* s = (KWStream*)arg;
	*s << domain << "|";
}
inline void count_domain_iterator(void* arg, const char* domain, void* vh)
{
	int* s = (int*)arg;
	(*s) += 1;
}
class KRefererAcl : public KAcl
{
public:
	KRefererAcl() {
		vhc = NULL;
		host_null = false;
		eq_host = false;
	}
	~KRefererAcl() {
		if (vhc) {
			delete vhc;
		}
	}
	KAcl* new_instance() override {
		return new KRefererAcl();
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		kgl_str_t value;
		if (!rq->get_http_value(_KS("Referer"), &value)) {
			return this->host_null;
		}
		if (!eq_host && !vhc) {
			return false;
		}
		KSafeUrl referer(new KUrl(true));
		if (!parse_url(value.data, value.len, referer.get()) || !referer->host) {
			return false;
		}
		if (eq_host) {
			// Compare with the client's Host, before any upstream host rewrite.
			const char* host = rq->sink->data.raw_url.host;
			if (!host && rq->sink->data.url) {
				host = rq->sink->data.url->host;
			}
			KString b1, b2;
			host = kgl_acl_host(host, b1);
			return host && strcasecmp(kgl_acl_host(referer->host, b2), host) == 0;
		}
		return  (vhc->find(referer->host) != NULL);
	}
	void get_display(KWStream &s) override {
		if (eq_host) {
			s << "EqHost";
			return;
		}
		int count = 0;
		s << "Host:";
		if (this->host_null) {
			count = 1;
		}
		if (vhc) {
			vhc->iterator(count_domain_iterator, &count);
		}
		s << count;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		eq_host = strcasecmp(attribute["referer"].c_str(), "EqHost") == 0;
		if (vhc) {
			delete vhc;
			vhc = NULL;
		}
		this->host_null = false;
		char* buf = strdup(attribute["host"].c_str());
		char* hot = buf;
		for (;;) {
			char* p = strchr(hot, '|');
			if (p) {
				*p = '\0';
				p++;
			}
			if (strcmp(hot, "-") == 0) {
				this->host_null = true;
			} else {
				if (vhc == NULL) {
					vhc = new KDomainMap;
				}
				vhc->bind(hot, this, kgl_bind_unique);
			}
			if (p == NULL) {
				break;
			}
			hot = p;
		}
		free(buf);
	}
	const char *get_module() const  override {
		return "referer";
	}
	void get_html(KWStream &s) override {
		s << "<input type='text' name='host' value='";
		if (this->host_null) {
			s << "-|";
		}
		if (vhc) {
			vhc->iterator(referer_domain_iterator, &s);
		}
		s << "' placeHolder='-|abc.com|*.abc.com'>";
		s << "<label><input type='checkbox' name='referer' value='EqHost'";
		if (eq_host) {
			s << " checked";
		}
		s << ">EqHost</label>";
	}
private:
	KDomainMap* vhc;
	bool host_null;
	bool eq_host;
};
#endif
