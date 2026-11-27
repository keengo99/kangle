#ifndef KEARLYHINTSMARK_H
#define KEARLYHINTSMARK_H
#include "KMark.h"
#include "KHttpRequest.h"

// Request rules can override the virtual host's opt-in policy.
class KEarlyHintsMark : public KMark
{
public:
	KMark* new_instance() override { return new KEarlyHintsMark; }
	const char* get_module() const override { return "early_hints"; }
	uint32_t process(KHttpRequest* rq, KHttpObject*, KSafeSource&) override {
		rq->ctx.early_hints = mode;
		rq->ctx.early_hints_set = 1;
		return KF_STATUS_REQ_TRUE;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto value = xml->attr()["value"];
		mode = value == "h2" ? 2 : (value == "on" || value == "1" ? 1 : 0);
	}
	void get_display(KWStream& s) override { s << (mode == 2 ? "h2" : (mode ? "on" : "off")); }
	void get_html(KWStream& s) override {
		s << "<select name='value'>";
		const char* values[] = { "off", "on", "h2" };
		for (unsigned i = 0; i < 3; ++i) {
			s << "<option value='" << values[i] << "'" << (mode == i ? " selected" : "") << ">" << values[i] << "</option>";
		}
		s << "</select>";
	}
private:
	unsigned mode = 1;
};
#endif
