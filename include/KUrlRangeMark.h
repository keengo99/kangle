#ifndef KURLRANGEMARK_H
#define KURLRANGEMARK_H

#include <cerrno>
#include <cstdlib>

#include "KHttpRequest.h"
#include "KMark.h"
#include "KReg.h"

class KUrlRangeMark final : public KMark {
public:
	KUrlRangeMark() : from_valid(false), to_valid(false) {}

	KMark* new_instance() override { return new KUrlRangeMark; }
	const char* get_module() const override { return "url_range"; }

	uint32_t process(KHttpRequest* rq, KHttpObject*, KSafeSource&) override {
		if (!from_valid || rq->get_range()) {
			return KF_STATUS_REQ_FALSE;
		}
		KStringBuf url;
		rq->sink->data.url->GetUrl(url);
		KRegSubString* match = range_from.matchSubString(url.c_str(), (int)url.size(), 0);
		if (!match) return KF_STATUS_REQ_FALSE;
		int64_t from = -1;
		int64_t to = -1;
		bool ok = parse_number(match->getString(1), from);
		const char* captured_to = match->getString(2);
		if (ok && captured_to) {
			ok = parse_number(captured_to, to);
		}
		bool have_captured_to = captured_to != nullptr;
		delete match;
		if (ok && !have_captured_to && to_valid) {
			match = range_to.matchSubString(url.c_str(), (int)url.size(), 0);
			if (match) {
				const char* value = match->getString(1);
				if (value) ok = parse_number(value, to);
				delete match;
			}
		}
		if (!ok || from < 0 || (to >= 0 && to < from)) {
			return KF_STATUS_REQ_FALSE;
		}
		KStringBuf value;
		value << "bytes=" << from << "-";
		if (to >= 0) value << to;
		if (!rq->sink->parse_header<const char*>(_KS("Range"), value.c_str(),
			(int)value.size(), false)) {
			return KF_STATUS_REQ_FALSE;
		}
		KBIT_SET(rq->sink->data.raw_url.flags, KGL_URL_RANGED);
		return KF_STATUS_REQ_TRUE;
	}

	void get_display(KWStream& s) override {
		s << range_from_text << " " << range_to_text;
	}

	void get_html(KWStream& s) override {
		s << "range_from:<input name='range_from' value='" << range_from_text
		  << "'> range_to:<input name='range_to' value='" << range_to_text << "'>";
	}

	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attr = xml->attr();
		range_from_text = attr["range_from"];
		range_to_text = attr["range_to"];
		from_valid = range_from.setModel(range_from_text.c_str(), KGL_PCRE_CASELESS);
		to_valid = !range_to_text.empty() &&
			range_to.setModel(range_to_text.c_str(), KGL_PCRE_CASELESS);
	}

private:
	static bool parse_number(const char* text, int64_t& value) {
		if (!text || !*text || *text == '-') return false;
		errno = 0;
		char* end = nullptr;
		long long parsed = strtoll(text, &end, 10);
		if (errno == ERANGE || !end || *end != '\0' || parsed < 0) return false;
		value = (int64_t)parsed;
		return true;
	}

	KReg range_from;
	KReg range_to;
	KString range_from_text;
	KString range_to_text;
	bool from_valid;
	bool to_valid;
};

#endif
