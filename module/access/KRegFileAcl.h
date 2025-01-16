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
#ifndef KREGFILEACL_H_
#define KREGFILEACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KStringBuf.h"
class KRegFileAcl: public KAcl {
public:
	KRegFileAcl() {
		nc = true;
	}
	virtual ~KRegFileAcl() {
	}
	void get_html(KWStream& s) override {
		s << "<input name='file' value='";
		KRegFileAcl *urlAcl = (KRegFileAcl *) (this);
		if (urlAcl) {
			urlAcl->get_display(s);
		}
		s << "'>";
		s << "<input name='nc' value='1' type='checkbox' ";
		if (urlAcl && urlAcl->nc) {
			s << "checked";
		}
		s << ">nc";
	}
	KAcl *new_instance() override {
		return new KRegFileAcl();
	}
	const char *get_module() const override {
		return "reg_file";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		if(rq->file==NULL){
			return false;
		}
		const char *filename = rq->file->getName();
		return path.match(filename,strlen(filename),0)>0;
	}
	void get_display(KWStream& s) override {
		s <<  path.getModel();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = KGL_PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		nc = (attribute["nc"]=="1");
		if (attribute["file"].size() > 0) {
			setPath(attribute["file"].c_str());
		}
	}
private:
	KReg path;
	bool nc;
};
class KRegFileNameAcl final: public KAcl {
public:
	KRegFileNameAcl() {
		nc = true;
	}
	virtual ~KRegFileNameAcl() {
	}
	void get_html(KWStream& s) override {
		s << "<input name='filename' value='";
		get_display(s);
		s << "'>";
		s << "<input name='nc' value='1' type='checkbox' ";
		if (nc) {
			s << "checked";
		}
		s << ">nc";
	}
	KAcl *new_instance() override {
		return new KRegFileNameAcl();
	}
	const char *get_module() const override {
		return "reg_filename";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		if(rq->file==NULL){
			return false;
		}
		const char *filename = rq->file->getName();
		const char *p = strrchr(filename,PATH_SPLIT_CHAR);
		if(p){
			p++;
			return path.match(p,strlen(p),0)>0;
		}
		return path.match(filename,strlen(filename),0)>0;
	}
	void get_display(KWStream& s) override {
		s <<  path.getModel();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = KGL_PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		nc = (attribute["nc"]=="1");
		if (attribute["filename"].size() > 0) {
			setPath(attribute["filename"].c_str());
		}
	}
private:
	KReg path;
	bool nc;
};

#endif 
