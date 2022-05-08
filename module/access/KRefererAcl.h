#ifndef KREFERERACL_H
#define KREFERERACL_H
#include "KAcl.h"
#include "KVirtualHostContainer.h"
inline void referer_domain_iterator(void *arg, const char *domain, void *vh)
{
	std::stringstream *s = (std::stringstream *)arg;
	*s << domain << "|";
}
inline void count_domain_iterator(void *arg, const char *domain, void *vh)
{
	int *s = (int *)arg;
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
	KAcl *newInstance() {
		return new KRefererAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		KHttpHeader *tmp = rq->sink->data.GetHeader();
		while (tmp) {
			if (is_attr(tmp, kgl_expand_string("Referer"))) {
				KAutoUrl referer;
				if (!parse_url(tmp->val, referer.u)) {
					return false;
				}
				bool matched = false;
				if (vhc) {
					matched = (vhc->find(referer.u->host) != NULL);
				}
				return matched;
			}
			tmp = tmp->next;
		}
		return this->host_null;
	}
	std::string getDisplay() {
		int count = 0;
		std::stringstream s;
		s << "Host:";
		if (this->host_null) {
			count = 1;
		}
		if (vhc) {
			vhc->iterator(count_domain_iterator, &count);
		}
		s << count;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		if (vhc) {
			delete vhc;
			vhc = NULL;
		}
		this->host_null = false;
		char *buf = strdup(attribute["host"].c_str());
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot, '|');
			if (p) {
				*p = '\0';
				p++;
			}
			if (strcmp(hot, "-") == 0) {
				this->host_null = true;
			}
			else {
				if (vhc == NULL) {
					vhc = new KVirtualHostContainer;
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
	const char *getName() {
		return "referer";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input type='text' name='host' value='";
		if (this->host_null) {
			s << "-|";
		}
		if (vhc) {
			vhc->iterator(referer_domain_iterator, &s);
		}
		s << "' placeHolder='-|abc.com|*.abc.com'>";
		return s.str();
	}
	void buildXML(std::stringstream &s) {
		s << " host='";
		if (this->host_null) {
			s << "-|";
		}
		if (vhc) {
			vhc->iterator(referer_domain_iterator, &s);
		}
		s << "'>";
	}
private:
	KVirtualHostContainer *vhc;
	bool host_null;
};
#endif
