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
#ifndef KCACHECONTROLMARK_H_
#define KCACHECONTROLMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
class KCacheControlMark: public KMark {
public:
	KCacheControlMark() {
		max_age = 0;
		force = false;
		static_flag = false;
		must_revalidate = false;
	}
	virtual ~KCacheControlMark() {
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
#ifdef ENABLE_FORCE_CACHE
		if (force) {
			if (!obj->force_cache(static_flag)) {
				return false;
			}
		}
#endif
		if (!KBIT_TEST(obj->index.flags,ANSW_NO_CACHE)) {			
			if (max_age>0) {
				obj->data->i.max_age = max_age;
				//soft指标是否发送max-age头给客户
				//KBIT_SET(obj->index.flags,(soft?ANSW_HAS_EXPIRES:ANSW_HAS_MAX_AGE));
			}
			if (must_revalidate) {
				KBIT_SET(obj->index.flags,OBJ_MUST_REVALIDATE);
			}
		}
		return KF_STATUS_REQ_TRUE;
	}
	void get_display(KWStream& s) override {
		s << "max_age:" << max_age;
		if (static_flag) {
			s << " static";
		} else if (force) {
			s << " force";
		}
		if (must_revalidate) {
			s << " must_revalidate";
		}
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		max_age = atoi(attribute["max_age"].c_str());
		force = (attribute["force"] == "1");
		if(attribute["static"]=="on" || attribute["static"]=="1"){
			static_flag = true;
			force = true;
		}else{
			static_flag = false;
		}
		must_revalidate = (attribute["must_revalidate"]=="1");
	}
	void get_html(KWStream& s) override {
		KCacheControlMark *mark = (KCacheControlMark *) this;
		s << "max_age: <input type=text name=max_age size=6 value='";
		if (mark) {
			s << mark->max_age;
		}
		s << "'><input type=checkbox name='static' value='1' ";
		if(mark && mark->static_flag){
			s << "checked";
		}
		s << ">" << klang["static"];

		s << "<input type=checkbox name='force' value='1' ";
		if(mark && mark->force){
			s << "checked";
		}
		s << ">force";

		s << "<input type=checkbox name='must_revalidate' value='1' ";
		if(mark && mark->must_revalidate){
			s << "checked";
		}
		s << ">must_revalidate";
	}
	KMark * new_instance() override {
		return new KCacheControlMark();
	}
	const char *getName() override {
		return "cache_control";
	}
private:
	unsigned max_age;
	bool force;
	bool static_flag;	
	bool must_revalidate;
};
#endif /*KRESPONSEFLAGMARK_H_*/
