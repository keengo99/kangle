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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		uint32_t result = KF_STATUS_REQ_FALSE;
		if (flag > 0) {
			KBIT_SET(obj->index.flags, flag);
			result = KF_STATUS_REQ_TRUE;
		}
		 if (compress) {
			obj->need_compress = 1;
			result = KF_STATUS_REQ_TRUE;
		}
		if (identity_encoding && !obj->IsContentEncoding()) {
			obj->uk.url->accept_encoding = (uint8_t)(~0);
			result = KF_STATUS_REQ_TRUE;
		}
		return result;
	}
	void get_display(KWStream& s) override {
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
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
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
	void get_html(KWStream& s) override {
		s << "<input type=text name=flagvalue value='";	
		get_display(s);
		s << "'>(available:compress, nocache, nodiskcache, identity_encoding)";
	}
	KMark * new_instance() override {
		return new KResponseFlagMark();
	}
	const char *getName() override {
		return "response_flag";
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
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		if (no_extend) {
			KBIT_SET(rq->ctx.filter_flags,RQ_NO_EXTEND);
		} else {
			KBIT_CLR(rq->ctx.filter_flags,RQ_NO_EXTEND);
		}
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream &s) override {
		if(no_extend){
			s << klang["no_extend"];
		} else {
			s << klang["normal"];
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		if(attribute["no_extend"]=="1"){
			no_extend = true;
		} else {
			no_extend = false;
		}
	
	}
	void get_html(KWStream &s)override {
		KExtendFlagMark *m_chain=(KExtendFlagMark *)this;
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
	}
	KMark *new_instance() override {
		return new KExtendFlagMark();
	}
	const char *getName()override {
		return "extend_flag";
	}
private:
	bool no_extend;
};


#endif /*KRESPONSEFLAGMARK_H_*/
