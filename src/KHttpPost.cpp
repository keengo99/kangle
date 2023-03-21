#include <string.h>
#include "KHttpPost.h"
#include "kmalloc.h"
#include "KUrlParser.h"
#include "KFileName.h"
bool KHttpPost::parseUrlParam(char* param, size_t len, KUrlValue* uv) {
	return uv->parse(param, len);
#if 0
	char* name;
	char* value;
	char* tmp;
	char* msg;
	char* ptr;
	for (size_t i = 0; i < strlen(param); i++) {
		if (param[i] == '\r' || param[i] == '\n') {
			param[i] = 0;
			break;
		}
	}
	tmp = param;
	char split = '&';
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		char* p = strchr(msg, '=');
		if (p == NULL) {
			value = (char*)"";
		} else {
			*p = '\0';
			value = p + 1;
			if (urldecode) {
				url_decode(value, 0, NULL, true);
			}
		}
		name = msg;
		if (urldecode) {
			url_decode(name, 0, NULL, true);
		}
		uv->put(name, value);
	}
	return true;
#endif
}
KHttpPost::KHttpPost() {
	buffer = NULL;
}

KHttpPost::~KHttpPost(void) {
	if (buffer) {
		xfree(buffer);
	}
}
bool KHttpPost::init(int totalLen) {
	if (totalLen > MAX_POST_SIZE) {
		return false;
	}
	this->totalLen = totalLen;
	buffer = (char*)xmalloc(totalLen + 1);
	hot = buffer;
	return buffer != NULL;

}
bool KHttpPost::addData(const char* data, int len) {
	int used = (int)(hot - buffer);
	if (totalLen - used < len) {
		return false;
	}
	kgl_memcpy(hot, data, len);
	hot += len;
	return true;
}
bool KHttpPost::readData(KRStream* st) {
	int used = (int)(hot - buffer);
	int leftRead = totalLen - used;
	for (;;) {
		if (leftRead <= 0) {
			return true;
		}
		int read_len = st->read(hot, leftRead);
		if (read_len <= 0) {
			return false;
		}
		leftRead -= read_len;
		hot += read_len;
	}
}
bool KHttpPost::parse(KUrlValue* uv) {
	*hot = '\0';
	//printf("post data=[%s]\n",buffer);
	assert(this->totalLen >= hot - buffer);
	return parseUrlParam(buffer, hot-buffer, uv);
}
