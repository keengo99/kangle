/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef KSTATUSCODEMARK_H
#define KSTATUSCODEMARK_H

#include "KFetchObject.h"
#include "KHttpRequest.h"
#include "KMark.h"

class KStatusCodeSource final : public KFetchObject {
public:
	explicit KStatusCodeSource(uint16_t code) : KFetchObject(0), code(code) {
	}

	KGL_RESULT Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) override {
		return out->f->error(out->ctx, code, _KS("status code mark"));
	}

private:
	uint16_t code;
};

class KStatusCodeMark final : public KMark {
public:
	KStatusCodeMark() : code(403) {
	}

	KMark* new_instance() override {
		return new KStatusCodeMark();
	}

	const char* get_module() const override {
		return "status_code";
	}

	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		KBIT_SET(rq->ctx.filter_flags, RF_NO_CACHE);
		fo.reset(new KStatusCodeSource(code));
		return KF_STATUS_REQ_TRUE;
	}

	void get_display(KWStream& s) override {
		s << code;
	}

	void get_html(KWStream& s) override {
		s << "HTTP status code (200-599): <input name='code' value='" << code << "'>";
	}

	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		int value = xml->attr().get_int("code", 403);
		code = (value >= 200 && value <= 599) ? static_cast<uint16_t>(value) : 403;
	}

private:
	uint16_t code;
};

#endif
