/*
 * KPathRedirect.cpp
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */
#include "utils.h"
#include "KPathRedirect.h"
#include "kmalloc.h"
void KRedirectMethods::setMethod(const char *methodstr)
{
	memset(methods, 0, sizeof(methods));
	if (methodstr == NULL || *methodstr == '\0') {
		return;
	}
	if (strcmp(methodstr, "*") == 0) {
		for (int i = 0; i < MAX_METHOD; i++) {
			methods[i] = true;
		}
		return;
	}
	char *buf = strdup(methodstr);
	char *hot = buf;
	for (;;) {
		char *p = strchr(hot, ',');
		if (p) {
			*p = '\0';
		}
		int meth = KHttpKeyValue::getMethod(hot);
		if (meth > 0) {
			methods[meth] = 1;
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	xfree(buf);
}
KPathRedirect::KPathRedirect(const char *path, KRedirect *rd) : KBaseRedirect(rd, KGL_CONFIRM_FILE_NEVER) {
	path_len = (int)strlen(path);
	this->path = (char *)xmalloc(path_len + 1);
	kgl_memcpy(this->path, path, path_len+1);
	if (*path == '~') {
		reg = new KReg;
#ifdef _WIN32
		int flag = PCRE_CASELESS;
#else
		int flag = 0;
#endif
		reg->setModel(path + 1, flag);
	} else {
		reg = NULL;
	}
}

KPathRedirect::~KPathRedirect() {
	if (path) {
		xfree(path);
	}
	if (reg) {
		delete reg;
	}
}

bool KPathRedirect::match(const char *path, int len) {
	if (reg) {
		return reg->match(path, len, 0) > 0;
	}
	return filencmp(path, this->path, path_len) == 0;
}
