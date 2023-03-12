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
#ifndef KREGPATHACL_H_
#define KREGPATHACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KStringBuf.h"
class KRegPathAcl: public KAcl {
public:
	KRegPathAcl() {
		raw = false;
		nc = true;
	}
	virtual ~KRegPathAcl() {
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input name=path value='";
		KRegPathAcl *urlAcl = (KRegPathAcl *) (model);
		if (urlAcl) {
			s << urlAcl->path.getModel();
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
		return new KRegPathAcl();
	}
	const char *getName() override {
		return "reg_path";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		if (raw) {
			return path.match(rq->sink->data.raw_url->path,strlen(rq->sink->data.raw_url->path),0)>0;
		}
		return path.match(rq->sink->data.url->path,strlen(rq->sink->data.url->path),0)>0;
	}
	void get_display(KWStream& s) override {
		if (raw) {
			s << "raw:";
		}
		s << path.getModel();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		nc = (attribute["nc"]=="1");
		if (attribute["path"].size() > 0) {
			setPath(attribute["path"].c_str());
		}
		if (attribute["raw"]=="1") {
			raw = true;
		} else {
			raw = false;
		}
	}
private:
	KReg path;
	bool raw;
	bool nc;
};
class KRegParamAcl: public KAcl {
public:
	KRegParamAcl() {
		raw = false;
		nc = true;
	}
	virtual ~KRegParamAcl() {
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input name='param' value='";
		KRegParamAcl *urlAcl = (KRegParamAcl *) (model);
		if (urlAcl) {
			s << urlAcl->path.getModel();
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
		return new KRegParamAcl();
	}
	const char *getName() override {
		return "reg_param";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		char *param = (raw?rq->sink->data.raw_url->param:rq->sink->data.url->param);
		if (param==NULL || *param=='\0') {
			return false;
		}
		return path.match(param,strlen(param),0)>0;
	}
	void get_display(KWStream& s) override {
		if (raw) {
			s << "raw:";
		}
		s << path.getModel();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		nc = (attribute["nc"]=="1");
		if (attribute["param"].size() > 0) {
			setPath(attribute["param"].c_str());
		}
		if (attribute["raw"]=="1") {
			raw = true;
		} else {
			raw = false;
		}
	}
	
private:
	KReg path;
	bool raw;
	bool nc;
};
#endif /*KURLACL_H_*/
