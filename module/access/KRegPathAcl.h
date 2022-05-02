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
	std::string getHtml(KModel *model) {
		std::stringstream s;
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
		return s.str();
	}
	KAcl *newInstance() {
		return new KRegPathAcl();
	}
	const char *getName() {
		return "reg_path";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (raw) {
			return path.match(rq->sink->data.raw_url.path,strlen(rq->sink->data.raw_url.path),0)>0;
		}
		return path.match(rq->sink->data.url->path,strlen(rq->sink->data.url->path),0)>0;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (raw) {
			s << "raw:";
		}
		s << path.getModel();
		return s.str();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
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
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if (strlen(path.getModel()) == 0) {
			setPath(character);
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
		s << " path='" << path.getModel() << "'>";
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
	std::string getHtml(KModel *model) {
		std::stringstream s;
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
		return s.str();
	}
	KAcl *newInstance() {
		return new KRegParamAcl();
	}
	const char *getName() {
		return "reg_param";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		char *param = (raw?rq->sink->data.raw_url.param:rq->sink->data.url->param);
		if (param==NULL || *param=='\0') {
			return false;
		}
		return path.match(param,strlen(param),0)>0;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (raw) {
			s << "raw:";
		}
		s << path.getModel();
		return s.str();
	}
	bool setPath(const char *value) {
		int flag = 0;
		if (nc) {
			flag = PCRE_CASELESS;
		}
		path.setModel(value,flag);
		return true;
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) {
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
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if (strlen(path.getModel()) == 0) {
			setPath(character);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " param='" << path.getModel() << "'";
		if (raw) {
			s << " raw='1'";
		}
		if (nc) {
			s << " nc='1'";
		}
		s << ">";
	}
private:
	KReg path;
	bool raw;
	bool nc;
};
#endif /*KURLACL_H_*/
