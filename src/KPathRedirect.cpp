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
	meths.reset();
	if (methodstr == NULL || *methodstr == '\0') {
		return;
	}
	if (strcmp(methodstr, "*") == 0) {
		for (int i = 0; i < MAX_METHOD; i++) {
			meths.set(i, true);
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
		int meth = KHttpKeyValue::get_method(hot, (int)strlen(hot));
		if (meth > 0) {
			meths.set(meth, true);
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	xfree(buf);
}
KPathRedirect::KPathRedirect(const char *path, KRedirect *rd) : KBaseRedirect(rd, KConfirmFile::Never) {
	path_len = (int)strlen(path);
	this->path = (char *)xmalloc(path_len + 1);
	kgl_memcpy(this->path, path, path_len+1);
	if (*path == '~') {
		reg = new KReg;
#ifdef _WIN32
		int flag = KGL_PCRE_CASELESS;
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
