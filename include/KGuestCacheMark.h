#ifndef KGUESTCACHEMARK_H
#define KGUESTCACHEMARK_H

#include <climits>

#include "KHttpObject.h"
#include "KHttpRequest.h"
#include "KMark.h"

class KGuestCacheMark final : public KMark {
public:
	KGuestCacheMark()
		: max_age(0), last_modified(false), soft(false),
		  must_revalidate(false), skip_set_cookie(false) {}

	KMark* new_instance() override { return new KGuestCacheMark; }
	const char* get_module() const override { return "guest_cache"; }

	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource&) override {
		if (!obj || !KBIT_TEST(rq->ctx.filter_flags, RF_GUEST) ||
			!KBIT_TEST(obj->index.flags, ANSW_NO_CACHE)) {
			return KF_STATUS_REQ_FALSE;
		}
		if (!skip_set_cookie &&
			(obj->find_header(_KS("Set-Cookie")) || obj->find_header(_KS("Set-Cookie2")))) {
			return KF_STATUS_REQ_FALSE;
		}
#ifdef ENABLE_FORCE_CACHE
		if (!obj->force_cache(last_modified)) {
			return KF_STATUS_REQ_FALSE;
		}
#else
		return KF_STATUS_REQ_FALSE;
#endif
		if (skip_set_cookie) {
			obj->remove_http_header(_KS("Set-Cookie"));
			obj->remove_http_header(_KS("Set-Cookie2"));
		}
		if (max_age > 0) {
			obj->data->i.max_age = max_age;
			KBIT_SET(obj->index.flags, soft ? ANSW_HAS_EXPIRES : ANSW_HAS_MAX_AGE);
		}
		if (must_revalidate) {
			KBIT_SET(obj->index.flags, OBJ_MUST_REVALIDATE);
		}
		KBIT_SET(obj->index.flags, OBJ_IS_GUEST);
		return KF_STATUS_REQ_TRUE;
	}

	void get_display(KWStream& s) override {
		s << "max_age:" << max_age;
		if (last_modified) s << " last_modified";
		if (soft) s << " soft";
		if (must_revalidate) s << " must_revalidate";
		if (skip_set_cookie) s << " skip_set_cookie";
	}

	void get_html(KWStream& s) override {
		s << "max_age:<input type=text name=max_age size=6 value='" << max_age
		  << "'><input type=checkbox name=last_modified value='1' "
		  << (last_modified ? "checked" : "") << ">last_modified"
		  << "<input type=checkbox name=soft value='1' " << (soft ? "checked" : "")
		  << ">soft<input type=checkbox name=must_revalidate value='1' "
		  << (must_revalidate ? "checked" : "")
		  << ">must_revalidate<input type=checkbox name=skip_set_cookie value='1' "
		  << (skip_set_cookie ? "checked" : "") << ">skip_set_cookie";
	}

	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attr = xml->attr();
		int64_t age = attr.get_int("max_age", 0);
		max_age = age > 0 && age <= UINT_MAX ? (unsigned)age : 0;
		last_modified = attr["last_modified"] == "1";
		soft = attr["soft"] == "1";
		must_revalidate = attr["must_revalidate"] == "1";
		skip_set_cookie = attr["skip_set_cookie"] == "1";
	}

private:
	unsigned max_age;
	bool last_modified;
	bool soft;
	bool must_revalidate;
	bool skip_set_cookie;
};

#endif
