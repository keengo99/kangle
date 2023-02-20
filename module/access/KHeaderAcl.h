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
	virtual ~KHeaderAcl() {
	}
	std::string getHtml(KModel* model) override {
		std::stringstream s;
		s << "attr:<input name=header value='";
		KHeaderAcl* urlAcl = (KHeaderAcl*)(model);
		if (urlAcl) {
			s << urlAcl->header;
		}
		s << "'>";
		s << "val:<input name=val value='";
		if (urlAcl) {
			s << urlAcl->reg.getModel();
		}
		s << "'>\n<input type=checkbox name='nc' value='1' ";
		if (urlAcl == NULL || urlAcl->nc) {
			s << "checked";
		}
		s << ">nc";
		return s.str();
	}
	const char* getName() override {
		return "header";
	}
	bool match_header(KHttpHeader* next) {
		while (next) {
			if (kgl_is_attr(next, header.c_str(), header.size())) {
				if (reg.match(next->buf+next->val_offset, next->val_len, 0) >= 0) {
					return true;
				}
			}
			next = next->next;
		}
		return false;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << header << "/" << reg.getModel();
		if (nc) {
			s << " [nc]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html) override {
		header = attribute["header"];
		nc = attribute["nc"] == "1";
		reg.setModel(attribute["val"].c_str(), nc ? PCRE_CASELESS : 0);
		regLen = strlen(reg.getModel());
	}
	bool startCharacter(KXmlContext* context, char* character, int len) override {
		if (len > 0) {
			reg.setModel(character, nc ? PCRE_CASELESS : 0);
			regLen = strlen(reg.getModel());
		}
		return true;
	}
	void buildXML(std::stringstream& s) override {
		s << "header='" << KXml::param(header.c_str()) << "' nc='" << (nc ? 1 : 0) << "'";
		s << ">" << CDATA_START << reg.getModel() << CDATA_END;
	}
private:
	std::string header;
	KReg reg;
	int regLen;
	bool nc;
};

class KHeaderMapAcl : public KMultiAcl
{
public:
	KHeaderMapAcl() {

	}
	virtual ~KHeaderMapAcl() {
	}
	std::string getHtml(KModel* model) override {
		std::stringstream s;
		s << "name:<input name=name value='";
		KHeaderMapAcl* urlAcl = (KHeaderMapAcl*)(model);
		if (urlAcl) {
			s << urlAcl->name;
		}
		s << "'>value:" << KMultiAcl::getHtml(model);
		return s.str();
	}
	KAcl* new_instance() override
	{
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
	std::string getDisplay() override {
		std::stringstream s;
		s << name << ":" << KMultiAcl::getDisplay();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string>& attribute, bool html) override {
		name = attribute["name"];
		KMultiAcl::editHtml(attribute, html);
	}

	void buildXML(std::stringstream& s) override {
		s << "name='" << KXml::param(name.c_str()) << "' ";
		KMultiAcl::buildXML(s);
	}
private:
	std::string name;

};
#endif /*KHEADERACL_H_*/
