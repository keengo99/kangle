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
		contentRange = false;
	}
	virtual ~KContentLengthAcl() {
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
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
		s << "<input type=checkbox name='content_range' value='1' ";
		if (acl && acl->contentRange) {
			s << "checked";
		}
		s << ">content range";
		return s.str();
	}
	KAcl *newInstance() {
		return new KContentLengthAcl();
	}
	const char *getName() {
		return "content_length";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		INT64 content_length = obj->getTotalContentSize();
		if (content_length < minlen) {
			return false;
		}
		if (maxlen>0 && content_length > maxlen) {
			return false;
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << get_size(minlen) << "," << get_size(maxlen);
		if (contentRange) {
			s << "[r]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html){
		minlen = get_size(attribute["min"].c_str());
		maxlen = get_size(attribute["max"].c_str());
		contentRange = (attribute["content_range"] == "1");
	}
	void buildXML(std::stringstream &s) {
		s << " min='" << get_size(minlen) << "' max='" << get_size(maxlen) << "' ";
		if (contentRange) {
			s << "content_range='1'";
		}
		s << ">";
	}
private:
	INT64 minlen;
	INT64 maxlen;
	bool contentRange;
};

#endif /*KCONTENTLENGTHACL_H_*/
