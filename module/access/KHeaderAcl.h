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
#ifndef KHEADERACL_H_
#define KHEADERACL_H_
#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KMultiAcl.h"

class KHeaderAcl : public KAcl
{
public:
	KHeaderAcl() {
		nc = true;
	}
	virtual ~KHeaderAcl() {}
	void get_html(KModel* model, KWStream& s) override {
		s << "attr:<input name=header value='";
		KHeaderAcl* urlAcl = (KHeaderAcl*)(model);
		if (urlAcl) {
			s << urlAcl->header;
		}
		s << "'>";
		s << "val:<input name='" << khttpd::KXmlNodeBody::text_as_attribute_name << "' value = '";
		if (urlAcl) {
			s << urlAcl->reg.getModel();
		}
		s << "'>\n<input type=checkbox name='nc' value='1' ";
		if (urlAcl == NULL || urlAcl->nc) {
			s << "checked";
		}
		s << ">nc";
	}
	const char* getName() override {
		return "header";
	}
	bool match_header(KHttpHeader* next) {
		while (next) {
			if (kgl_is_attr(next, header.c_str(), header.size())) {
				if (reg.match(next->buf + next->val_offset, next->val_len, 0) >= 0) {
					return true;
				}
			}
			next = next->next;
		}
		return false;
	}
	void get_display(KWStream& s) override {
		s << header << "/" << reg.getModel();
		if (nc) {
			s << " [nc]";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		header = attribute["header"];
		nc = attribute["nc"] == "1";
		auto text = xml->get_text();
		if (!text.empty()) {
			reg.setModel(text.c_str(), nc ? PCRE_CASELESS : 0);
		} else if (!attribute["val"].empty()) {
			reg.setModel(attribute["val"].c_str(), nc ? PCRE_CASELESS : 0);
		}
	}
private:
	KString header;
	KReg reg;
	bool nc;
};

class KHeaderMapAcl : public KMultiAcl
{
public:
	KHeaderMapAcl() {

	}
	virtual ~KHeaderMapAcl() {}
	void get_html(KModel* model, KWStream& s) override {
		s << "name:<input name=name value='";
		KHeaderMapAcl* urlAcl = (KHeaderMapAcl*)(model);
		if (urlAcl) {
			s << urlAcl->name;
		}
		s << "'>value:";
		KMultiAcl::get_html(model, s);
	}
	KAcl* new_instance() override {
		return new KHeaderMapAcl;
	}
	const char* getName() override {
		return "header_map";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		KHttpHeader* header = rq->sink->data.get_header();
		std::map<std::string, bool>::iterator it;
		while (header) {
			if (kgl_is_attr(header, name.c_str(), name.size())) {
				if (KMultiAcl::match(header->buf + header->val_offset)) {
					return true;
				}
			}
			header = header->next;
		}
		return false;
	}
	void get_display(KWStream& s) override {
		s << name << ":";
		KMultiAcl::get_display(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		name = attribute["name"];
		KMultiAcl::parse_config(xml);
	}
private:
	KString name;

};
#endif /*KHEADERACL_H_*/
