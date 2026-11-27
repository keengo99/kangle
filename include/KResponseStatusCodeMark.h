#ifndef KRESPONSESTATUSCODEMARK_H
#define KRESPONSESTATUSCODEMARK_H

#include "KHttpObject.h"
#include "KMark.h"

class KResponseStatusCodeMark final : public KMark {
public:
	KResponseStatusCodeMark() : code(403) {}
	KMark* new_instance() override { return new KResponseStatusCodeMark; }
	const char* get_module() const override { return "status_code"; }

	uint32_t process(KHttpRequest*, KHttpObject* obj, KSafeSource&) override {
		if (!obj || !obj->data) return KF_STATUS_REQ_FALSE;
		obj->data->i.status_code = code;
		if (code < 200 || code == 204 || code >= 300) {
			KBIT_SET(obj->index.flags, OBJ_NOT_OK);
		}
		if (code >= 400) {
			KBIT_SET(obj->index.flags, ANSW_NO_CACHE | FLAG_DEAD);
		}
		return KF_STATUS_REQ_TRUE;
	}

	void get_display(KWStream& s) override { s << code; }
	void get_html(KWStream& s) override {
		s << "HTTP status code (200-599): <input name='code' value='" << code << "'>";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		int value = xml->attr().get_int("code", 403);
		code = value >= 200 && value <= 599 ? (uint16_t)value : 403;
	}

private:
	uint16_t code;
};

#endif
