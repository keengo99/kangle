#ifndef KBODYWSTREAM_H_INCLUDED
#define KBODYWSTREAM_H_INCLUDED
#include "KStream.h"
#include "ksapi.h"

class KBodyWStream : public KWStream
{
public:
	KBodyWStream() {
		body = { 0 };
	}
	~KBodyWStream() {
		assert(!body.ctx);
		if (body.ctx) {
			body.f->close(body.ctx, KGL_EUNKNOW);
		}
	}
	KGL_RESULT write_end(KGL_RESULT result) override {
		result = body.f->close(body.ctx, result);
		body = { 0 };
		return result;
	}
	kgl_response_body body;
	KGL_RESULT write_all(const char* buf, int len) override {
		if (len == 0) {
			return KGL_OK;
		}
		return body.f->write(body.ctx, buf, len);
	}
};
#endif

