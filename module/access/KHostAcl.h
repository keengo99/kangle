/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KHOSTACL_H_
#define KHOSTACL_H_

#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KVirtualHostContainer.h"
inline void multi_domain_iterator(void *arg,const char *domain,void *vh)
{
	KWStream*s = (KWStream*)arg;
	if (!s->empty()) {
		*s << "|";
	}
	*s << domain;
}
class KHostAcl: public KMultiAcl {
public:
	KHostAcl() {
		icase_can_change = false;
	}
	virtual ~KHostAcl() {
	}
	KAcl *new_instance() override  {
		return new KHostAcl();
	}
	const char* get_module() const override {
		return "host";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return KMultiAcl::match(rq->sink->data.url->host);
	}
};
class KWideHostAcl : public KAcl{
public:
	KWideHostAcl() {
		
	}
	virtual ~KWideHostAcl() {
	}
	KAcl *new_instance() override {
		return new KWideHostAcl();
	}
	const char *get_module() const override {
		return "wide_host";
	}
	void get_display(KWStream& s) override {
		this->getValList(s);
	}
	void get_html(KWStream &s) override {
		s << "<input name=v size=40 placeholder='abc.com|*.abc.com' value='";
		KWideHostAcl *acl = (KWideHostAcl *) (this);
		if (acl) {
			acl->getValList(s);
		}
		s << "'>";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		return vhc.find(rq->sink->data.url->host)!=NULL;		
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		vhc.clear();
		char *buf = strdup(attribute["v"].c_str());
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,'|');
			if (p!=NULL) {
				*p++ = '\0';
			}
			vhc.bind(hot,this,kgl_bind_unique);
			if (p==NULL) {
				break;
			}
			hot = p;
		}
		free(buf);
	}
private:
	void getValList(KWStream& s) {
		vhc.iterator(multi_domain_iterator,&s);
		return;
	}
	KDomainMap vhc;
};

#endif /*KHOSTACL_H_*/
