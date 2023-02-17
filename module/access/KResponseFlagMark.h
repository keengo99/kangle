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
#ifndef KRESPONSEFLAGMARK_H_
#define KRESPONSEFLAGMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
class KResponseFlagMark : public KMark {
public:
	KResponseFlagMark() {
		flag=0;
		compress = false;
		identity_encoding = false;
	}
	virtual ~KResponseFlagMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo)override {
		bool result = false;
		if (flag > 0) {
			KBIT_SET(obj->index.flags, flag);
			result = true;
		}
		 if (compress) {
			obj->need_compress = 1;
			result = true;
		}
		if (identity_encoding && !obj->IsContentEncoding()) {
			obj->uk.url->accept_encoding = (uint8_t)(~0);
			result = true;
		}
		return result;
	}
	std::string getDisplay() override {
		std::stringstream s;
		if (compress) {
			s << "compress,";
		}
		if (KBIT_TEST(flag,FLAG_NO_NEED_CACHE)) {
			s << "nocache,";
		}
		if (KBIT_TEST(flag,FLAG_NO_DISK_CACHE)) {
			s << "nodiskcache,";
		}
		if (KBIT_TEST(flag, OBJ_CACHE_RESPONSE)) {
			s << "cache_response,";
		}
		if (identity_encoding) {
			s << "identity_encoding,";
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html)override {
		flag=0;
		compress = false;
		identity_encoding = false;
		const char *flagStr=attribute["flagvalue"].c_str();
		char *buf = strdup(flagStr);
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,',');
			if (p) {
				*p = '\0';
			}
			if (strcasecmp(hot, "compress") == 0) {
				compress = true;
			} else if (strcasecmp(hot,"gzip")==0) {
				compress = true;
			}
			if (strcasecmp(hot, "identity_encoding") == 0) {
				identity_encoding = true;
			}
			if (strcasecmp(hot,"nocache")==0) {
				KBIT_SET(flag,FLAG_NO_NEED_CACHE);
			}
			if (strcasecmp(hot,"nodiskcache")==0) {
				KBIT_SET(flag,FLAG_NO_DISK_CACHE);
			}
			if (strcasecmp(hot, "cache_response") == 0) {
				KBIT_SET(flag, OBJ_CACHE_RESPONSE);
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		free(buf);
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "<input type=text name=flagvalue value='";
		if (model) {
			s << model->getDisplay();
		}

		s << "'>(available:compress, nocache, nodiskcache, identity_encoding)";
		return s.str();
	}
	KMark * new_instance() override {
		return new KResponseFlagMark();
	}
	const char *getName() override {
		return "response_flag";
	}
public:
	void buildXML(std::stringstream &s) override {
		s << " flagvalue='" << getDisplay() << "'>";
	}
private:
	int flag;
	bool compress;
	bool identity_encoding;
};

class KExtendFlagMark : public KMark {
public:
	KExtendFlagMark() {
		no_extend=true;
	}
	virtual ~KExtendFlagMark() {
	}

	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override {
		if (no_extend) {
			KBIT_SET(rq->ctx.filter_flags,RQ_NO_EXTEND);
		} else {
			KBIT_CLR(rq->ctx.filter_flags,RQ_NO_EXTEND);
		}
		return true;
	}
	std::string getDisplay() override {
		std::stringstream s;
		if(no_extend){
			s << klang["no_extend"];
		} else {
			s << klang["normal"];
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html) override {
		if(attribute["no_extend"]=="1"){
			no_extend = true;
		} else {
			no_extend = false;
		}
	
	}
	std::string getHtml(KModel *model)override {
		KExtendFlagMark *m_chain=(KExtendFlagMark *)model;
		std::stringstream s;
		s << "<input type=radio name='no_extend' value='1' ";
		if (m_chain==NULL || m_chain->no_extend) {
			s << "checked";
		}
		s << ">" << klang["no_extend"];
		s << "<input type=radio name='no_extend' value='0' ";
		if (m_chain && !m_chain->no_extend) {
			s << "checked";
		}
		s << ">" << klang["clear_no_extend"];
		return s.str();
	}
	KMark *new_instance() override {
		return new KExtendFlagMark();
	}
	const char *getName()override {
		return "extend_flag";
	}
public:
	void buildXML(std::stringstream &s) override {
		s << " no_extend='" << (no_extend?1:0) << "'>";
	}
private:
	bool no_extend;
};


#endif /*KRESPONSEFLAGMARK_H_*/
