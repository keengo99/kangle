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
#ifndef KURLACL_H_
#define KURLACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
class KUrlAcl : public KAcl {
public:
	KUrlAcl() {
		raw = false;
		nc = true;
	}
	virtual ~KUrlAcl() {
	}
	void get_html(KWStream& s) override {
		s << "<input name=url value='";
		KUrlAcl *urlAcl=(KUrlAcl *)(this);
		if (urlAcl) {
			s << urlAcl->reg.getModel();
		}
		s << "'>";
		
		s << "<input name='raw' value='1' type='checkbox' ";
		if (urlAcl && urlAcl->raw) {
			s << "checked";
		}
		s << ">raw";
		s << "<input name='nc' value='1' type='checkbox' ";
		if (urlAcl && urlAcl->nc) {
			s << "checked";
		}
		s << ">nc";
	}
	KAcl *new_instance() override {
		return new KUrlAcl();
	}
	const char *getName() override {
		return "url";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		KStringStream url;
		if (raw) {
			rq->sink->data.raw_url.GetUrl(url);
		} else {
			rq->sink->data.url->GetUrl(url);
		}
		if (reg.match(url.c_str(), url.size(), 0)>=0){
			return true;
		}
		return false;
	}
	void get_display(KWStream& s) override {
		if (raw) {
			s << "raw:";
		}
		s << reg.getModel();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		nc = (attribute["nc"]=="1");
		if(!attribute["url"].empty()){
			reg.setModel(attribute["url"].c_str(), (nc? KGL_PCRE_CASELESS :0));
		} else {
			auto text = xml->get_text();
			if (!text.empty()) {
				reg.setModel(text.c_str(), (nc ? KGL_PCRE_CASELESS : 0));
			}
		}
		if (attribute["raw"]=="1") {
			raw = true;
		} else {
			raw = false;
		}		
	}
private:
	KReg reg;
	bool raw;
	bool nc;
};

#endif /*KURLACL_H_*/
