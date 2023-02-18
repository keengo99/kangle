#include <string.h>
#include "KHttpAuth.h"
#include "KHttpRequest.h"
#include "kforwin32.h"
#include "kmalloc.h"
static const char *auth_types[] = { "Basic", "Digest" };
KHttpAuth::~KHttpAuth() {
	//printf("delete auth now\n");
}
int KHttpAuth::parseType(const char *type) {
	for (unsigned i = 0; i < TOTAL_AUTH_TYPE; i++) {
		if (strcasecmp(type, auth_types[i]) == 0) {
			return i;
		}
	}
	return 0;
}
const char *KHttpAuth::buildType(int type) {
	if (type >= 0 && type < TOTAL_AUTH_TYPE) {
		return auth_types[type];
	}
	return "unknow";
}
KGL_RESULT KAuthSource::Open(KHttpRequest* rq, kgl_input_stream* in, kgl_output_stream* out) {
	if (conf.auth_delay > 0 && KBIT_TEST(rq->sink->data.flags, RQ_HAS_PROXY_AUTHORIZATION | RQ_HAS_AUTHORIZATION)) {
		kfiber_msleep(conf.auth_delay * 1000);
	}
	uint16_t status_code = auth->GetStatusCode();
	out->f->write_status(out->ctx, status_code);
	auth->response_header(out);
	return out->f->write_header_finish(out->ctx, 0, nullptr);
}