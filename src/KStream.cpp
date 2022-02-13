/*
 * KStream.cpp
 *
 *  Created on: 2010-5-10
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#include <string.h>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#include "lib.h"
#include "kforwin32.h"
#include "KStream.h"
#include "kmalloc.h"
KConsole KConsole::out;
char *KRStream::readLine() {
	std::stringstream s;
	char buf[2];
	for (;;) {
		if (read(buf, 1) <= 0) {
			return NULL;
		}
		if (*buf == '\n') {
			break;
		}
		s << *buf;
	}
	return xstrdup(s.str().c_str());
}
bool KRStream::read_all(char *buf, int len) {
	while (len > 0) {
		int r = read(buf, len);
		if (r <= 0)
			return false;
		len -= r;
		buf += r;
	}
	return true;
}
StreamState KWStream::write_direct(char *buf, int len) {
	StreamState result = write_all(buf, len);
	xfree(buf);
	return result;
}
StreamState KWStream::write_all(const char *buf, int len) {
	while (len > 0) {
		int r = write(buf, len);
		if (r <= 0)
			return STREAM_WRITE_FAILED;
		len -= r;
		buf += r;
	}
	return STREAM_WRITE_SUCCESS;
}
StreamState KWStream::write_all(const char *buf) {
	return write_all(buf, strlen(buf));
}
int KConsole::write(const char *buf, int len) {
	char *str = (char *) xmalloc(len+1);
	kgl_memcpy(str, buf, len);
	str[len] = 0;
	printf("%s", str);
	xfree(str);
	return len;
}
