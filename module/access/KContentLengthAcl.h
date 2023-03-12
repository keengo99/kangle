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
#ifndef KCONTENTLENGTHACL_H_
#define KCONTENTLENGTHACL_H_

#include "KAcl.h"
class KContentLengthAcl : public KAcl {
public:
	KContentLengthAcl() {
		minlen=0;
		maxlen=-1;
	}
	virtual ~KContentLengthAcl() {
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "min:<input name=min value='";
		KContentLengthAcl *acl=(KContentLengthAcl *)(model);
		if (acl) {
			s << get_size(acl->minlen);
		}
		s << "'>,max:<input name=max value='";
		if (acl) {
			s << get_size(acl->maxlen);
		}
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KContentLengthAcl();
	}
	const char *getName() override {
		return "content_length";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		INT64 content_length = obj->getTotalContentSize();
		if (content_length < minlen) {
			return false;
		}
		if (maxlen>0 && content_length > maxlen) {
			return false;
		}
		return true;
	}
	void get_display(KWStream& s) override {
		s << get_size(minlen) << "," << get_size(maxlen);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		minlen = get_size(attribute["min"].c_str());
		maxlen = get_size(attribute["max"].c_str());
	}
private:
	INT64 minlen;
	INT64 maxlen;
};

#endif /*KCONTENTLENGTHACL_H_*/
