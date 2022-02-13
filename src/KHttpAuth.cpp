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
