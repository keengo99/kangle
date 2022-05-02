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
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=url value='";
		KUrlAcl *urlAcl=(KUrlAcl *)(model);
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
		return s.str();
	}
	KAcl *newInstance() {
		return new KUrlAcl();
	}
	const char *getName() {
		return "url";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		KStringBuf url;
		if (raw) {
			rq->sink->data.raw_url.GetUrl(url);
		} else {
			rq->sink->data.url->GetUrl(url);
		}
		if (reg.match(url.getString(), url.getSize(), 0)>=0){
			return true;
		}
		return false;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (raw) {
			s << "raw:";
		}
		s << reg.getModel();
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html){
		nc = (attribute["nc"]=="1");
		if(attribute["url"].size()>0){
			reg.setModel(attribute["url"].c_str(), (nc?PCRE_CASELESS:0));
		}
		if (attribute["raw"]=="1") {
			raw = true;
		} else {
			raw = false;
		}		
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if(len>0){
			reg.setModel(character, 0);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		if (raw) {
			s << " raw='1'";
		}
		if (nc) {
			s << " nc='1'";
		}
		s << ">" << CDATA_START << reg.getModel() << CDATA_END;
	}
private:
	KReg reg;
	bool raw;
	bool nc;
};

#endif /*KURLACL_H_*/
