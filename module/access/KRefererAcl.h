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
		if (!vhc) {
			return false;
		}
		KSafeUrl referer(new KUrl(true));
		if (!parse_url(value.data, value.len, referer.get())) {
			return false;
		}
		return  (vhc->find(referer->host) != NULL);
	}
	void get_display(KWStream &s) override {
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
	const char* getName() override {
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
	}
private:
	KDomainMap* vhc;
	bool host_null;
};
#endif
