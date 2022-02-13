/*
 * KHttpKeyValue.cpp
 *
 *  Created on: 2010-4-23
 *      Author: keengo
 */
#include <stdlib.h>
#include <string.h>
#include "KHttpKeyValue.h"
#include "kforwin32.h"
#include "kmalloc.h"
static const char *http_methods[MAX_METHOD] = {
		"UNSET",
		"OPTIONS",
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"TRACE",
		"PROPFIND",
		"PROPPATCH",
		"MKCOL",
		"COPY",
		"MOVE",
		"LOCK",
		"UNLOCK",
		"ACL",
		"REPORT",
		"VERSION_CONTROL",
		"CHECKIN",
		"CHECKOUT",
		"UNCHECKOUT",
		"SEARCH",
		"MKWORKSPACE",
		"UPDATE",
		"LABEL",
		"MERGE",
		"BASELINE_CONTROL",
		"MKACTIVITY",
		"CONNECT",
		"PURGE",
		"PATCH"
};
#if 0
static keyvalue http_status[] = {
		//把最常用的放在最前面，提高效率
		{ 200, "OK" },
		{ 304, "Not Modified" },
		{ 404, "Not Found" },
		{ 302, "Found" },
		{ 206, "Partial Content" },
		{ 500, "Internal Server Error" },
		{ 504,	"Gateway Timeout" },
		{ 401,	"Unauthorized" },
		{ 303, "See Other" },

		{ 100, "Continue" },
		{ 101, "Switching Protocols" },
		{ 102, "Processing" }, /* WebDAV */
		{ 201, "Created" },
		{ 202, "Accepted" },
		{ 203,	"Non-Authoritative Information" },
		{ 204, "No Content" },
		{ 205, "Reset Content" },
		{ 207, "Multi-status" }, /* WebDAV */
		{ 300, "Multiple Choices" },
		{ 301, "Moved Permanently" },
		{ 305,	"Use Proxy" },
		{ 306, "(Unused)" },
		{ 307, "Temporary Redirect" },
		{ 400, "Bad Request" },
		{ 402, "Payment Required" },
		{ 403,	"Forbidden" },
		{ 405, "Method Not Allowed" },
		{ 406, "Not Acceptable" },
		{ 407,	"Proxy Authentication Required" },
		{ 408, "Request Timeout" },
		{ 409, "Conflict" },
		{ 410, "Gone" },
		{ 411, "Length Required" },
		{ 412, "Precondition Failed" },
		{ 413, "Request Entity Too Large" },
		{ 414, "Request-URI Too Long" },
		{ 415, "Unsupported Media Type" },
		{ 416,	"Requested Range Not Satisfiable" },
		{ 417,	"Expectation Failed" },
		{ 422, "Unprocessable Entity" }, /* WebDAV */
		{ 423, "Locked" }, /* WebDAV */
		{ 424, "Failed Dependency" }, /* WebDAV */
		{ 426, "Upgrade Required" }, /* TLS */
		{ 451,"Unavailable For Legal Reasons"},
		{ 501, "Not Implemented" },
		{ 502,	"Bad Gateway" },
		{ 503, "Service Not Available" },
		{ 505, "HTTP Version Not Supported" },
		{ 507, "Insufficient Storage" }, /* WebDAV */
		{ -1, NULL }
};
const char *keyvalue_get_value(keyvalue *kv, int k) {
	int i;
	for (i = 0; kv[i].value; i++) {
		if (kv[i].key == k)
			return kv[i].value;
	}
	return NULL;
}
#endif
KHttpKeyValue::KHttpKeyValue() {

}
KHttpKeyValue::~KHttpKeyValue() {
}
const char *KHttpKeyValue::getMethod(int meth) {
	if (meth < 0 || meth >= MAX_METHOD) {
		return "";
	}
	return http_methods[meth];
}
int KHttpKeyValue::getMethod(const char *src) {
	for (int i = 1; i < MAX_METHOD; i++) {
		if (strcasecmp(src, http_methods[i]) == 0) {
			return i;
		}
	}
	return 0;
}
#if 0
const char *KHttpKeyValue::getStatus(int status) {
	const char *v = keyvalue_get_value(http_status, status);
	if(v){
		return v;
	}
	return "Unknow";
}
#endif
